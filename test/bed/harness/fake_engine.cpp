#include "fake_engine.h"

#include <save_xml.h>
#include <x4native.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <malloc.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace bed {
namespace {

Engine g;

// The value the registry slot points at. The mod dereferences the offset to get this.
void* g_registry_slot = reinterpret_cast<void*>(-1);
void* g_order_id_map_slot = reinterpret_cast<void*>(-1);
std::map<std::uint64_t, void*> g_trades;

// The fake image the mod resolves globals against: `x4n::exe_base()` is the base of this buffer, so
// the registry slot at its RVA is a real, writable pointer slot the SDK's own lookup will follow.
std::vector<std::uint8_t> g_image;
std::uint8_t* g_registry = nullptr;
std::uint8_t* g_faction_root = nullptr;
void* g_partner_faction = nullptr;

// The game's SaveXML namespace object: its entries inline, then their count, as X4Native reads them.
std::vector<std::uint8_t> g_savexml_namespace;
std::vector<std::string> g_savexml_names;

// --- The API the framework would hand the extension -----------------------------------------

int on_subscribe(const char* name, X4NativeEventCallback callback, void* userdata, void*) {
    const int id = g.next_id++;
    g.subscriptions.push_back(Engine::Subscription{id, name ? name : "", callback, userdata});
    return id;
}

void on_unsubscribe(int id) {
    for (auto it = g.subscriptions.begin(); it != g.subscriptions.end(); ++it) {
        if (it->id == id) {
            g.subscriptions.erase(it);
            return;
        }
    }
}

void on_raise(const char* name, void* data) { raise(name, data); }

// The game's own SaveXML lookups, over the fake namespace.
const char* game_format_xml_name(const int32_t* id) {
    const auto count = at<std::size_t>(g_savexml_namespace.data(), X4_SAVEXML_OFFSET_ENTRY_COUNT);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = g_savexml_namespace.data() + X4_SAVEXML_OFFSET_ENTRIES + i * X4_SAVEXML_ENTRY_SIZE;
        if (at<int32_t>(entry, X4_SAVEXML_ENTRY_OFFSET_ID) == *id) return at<const char*>(entry, X4_SAVEXML_ENTRY_OFFSET_NAME);
    }
    return "";
}

int32_t game_lookup_name(const char* name, const void*) {
    const auto count = at<std::size_t>(g_savexml_namespace.data(), X4_SAVEXML_OFFSET_ENTRY_COUNT);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = g_savexml_namespace.data() + X4_SAVEXML_OFFSET_ENTRIES + i * X4_SAVEXML_ENTRY_SIZE;
        const char* entry_name = at<const char*>(entry, X4_SAVEXML_ENTRY_OFFSET_NAME);
        if (std::strcmp(entry_name, name) == 0) return at<int32_t>(entry, X4_SAVEXML_ENTRY_OFFSET_ID);
    }
    return 0;
}

// X4Native's core answers a registration from its shared table against the game's namespace.
int32_t on_savexml_register(const char* name) {
    const void* ns = g.savexml_namespace_ready ? g_savexml_namespace.data() : nullptr;
    return x4n::SaveXml::register_name(ns, name);
}

int on_raise_lua(const char* name, const char* param) {
    g.lua.push_back(LuaCall{name ? name : "", param ? param : ""});
    return 0;
}

int on_register_bridge(const char*, const char*) { return 0; }

void on_log(int level, const char* message) {
    g.log.push_back(LogLine{level, message ? message : ""});
}

const char* on_game_version() { return "9.00"; }
const char* on_native_version() { return "0.9.0-bed"; }

int on_hook_before(const char* function, X4HookCallback callback, void* userdata, void*) {
    const int id = g.next_id++;
    g.hooks.push_back(HookEntry{id, function ? function : "", false, callback, userdata});
    return id;
}

int on_hook_after(const char* function, X4HookCallback callback, void* userdata, void*) {
    const int id = g.next_id++;
    g.hooks.push_back(HookEntry{id, function ? function : "", true, callback, userdata});
    return id;
}

void on_unhook(int id) {
    for (auto it = g.hooks.begin(); it != g.hooks.end(); ++it) {
        if (it->id == id) {
            g.hooks.erase(it);
            return;
        }
    }
}

// The framework installs a detour and hands back the trampoline to the original. The bed keeps the
// detour, so a scenario can call the game function the way the engine does and go through the same
// before-work-after chain the SDK builds.
// The game's rebuild of a list's check: the object in the slot goes through its vtable slot 0 with
// argument 1, and a fresh one compiled from the record takes its place.
void* original_filter_build(void* record) {
    void*& slot = at<void*>(record, g.offsets.blacklist_record_filter);
    if (void* old = slot) {
        slot = nullptr;
        void** vtable = at<void**>(old, 0);
        reinterpret_cast<void (*)(void*, int)>(vtable[0])(old, 1);
    }
    slot = make_engine_filter();
    return record;
}

// The list writer: one <blacklist> per list the registry holds, carrying the list's id.
void* original_list_export(void*, void* parent) {
    for (std::size_t i = 0; i < g.records.size(); ++i) {
        if (!g.records[i]) continue;
        void* node = make_save_node(kSaveBlacklist);
        set_save_attribute(node, kSaveId, std::to_string(i + 1).c_str());
        append_save_child(parent, node);
    }
    return nullptr;
}

// The list reader turns the elements back into lists; the engine's lists are already the bed's records.
void* original_list_import(void*, void*) { return nullptr; }

// The handler reads the command id and runs its trade search, which checks candidate partners.
void* original_find_offer(void*, void*, void*, void*) {
    if (g.find_offer_search) g.find_offer_search();
    return nullptr;
}
void* original_fetch(void*, int, int, int) { return g.fetched_filter; }

void* on_ensure_detour(const char* name, void* detour) {
    const std::string function = name ? name : "";
    for (const std::string& refused : g.detours_refused) {
        if (refused == function) return nullptr;
    }
    g.detours[function] = detour;
    if (function == "BlacklistFilterBuild") return reinterpret_cast<void*>(&original_filter_build);
    if (function == "BlacklistFetchForControllable") return reinterpret_cast<void*>(&original_fetch);
    if (function == "FindOfferAction") return reinterpret_cast<void*>(&original_find_offer);
    if (function == "BlacklistData_Export") return reinterpret_cast<void*>(&original_list_export);
    if (function == "BlacklistData_Import") return reinterpret_cast<void*>(&original_list_import);
    return reinterpret_cast<void*>(&original_filter_build);
}

void on_run_before(X4HookContext* context) { run_before(context->function_name, *context); }
void on_run_after(X4HookContext* context) { run_after(context->function_name, *context); }

void* on_get_function(const char*) { return nullptr; }
void* on_resolve_internal(const char*) { return nullptr; }

// --- The game's own functions ---------------------------------------------------------------

int game_type_from_name(void*, const void*) { return g.list_type_from_name; }

void* game_record_by_id(void*, int id) {
    if (id <= 0 || static_cast<std::size_t>(id) > g.records.size()) return nullptr;
    return g.records[static_cast<std::size_t>(id) - 1];
}

void* game_filter_build(void* record) { return record; }

void* game_fetch(void*, int, int, int) { return g.fetched_filter; }

void* game_find_offer(void*, void*, void*, void*) { return nullptr; }

void* game_alloc(size_t size, int, int, int, int alignment) {
    void* block = _aligned_malloc(size, static_cast<std::size_t>(alignment));
    if (block) std::memset(block, 0, size);
    return block;
}

void game_set_attribute(void* node, int32_t id, const char* value) { set_save_attribute(node, id, value); }

// A small vector of node pointers: four inline, then the heap; the capacity at +0x20, the count at +0x28.
void game_push_back(void* vector, void* const* item) {
    auto& capacity = at<std::size_t>(vector, 0x20);
    auto& count = at<std::size_t>(vector, 0x28);
    void** entries = capacity == 4 ? static_cast<void**>(vector) : at<void**>(vector, 0);
    if (count == capacity) {
        void** grown = new void*[capacity * 2]();
        for (std::size_t i = 0; i < count; ++i) grown[i] = entries[i];
        at<void**>(vector, 0) = grown;
        capacity *= 2;
        entries = grown;
    }
    entries[count++] = *item;
}

// The trade record carries handle-table ids, and the engine resolves them per type: 2 for the trade
// itself, 4 for a component such as its buyer or seller.
static std::map<std::uint64_t, void*> g_components;

void* game_resolve_handle(void*, uint64_t handle, int mask) {
    if (mask == 4) {
        const auto found = g_components.find(handle);
        return found == g_components.end() ? nullptr : found->second;
    }
    if (mask != 2) return nullptr;  // the trade slot the mod asks for
    const auto found = g_trades.find(handle);
    return found == g_trades.end() ? nullptr : found->second;
}

// The component registry: a UniverseID resolves to the component registered under it.
void* g_component_registry = reinterpret_cast<void*>(-1);

void* game_find_component(void*, uint64_t id, int) {
    const auto found = g_components.find(id);
    return found == g_components.end() ? nullptr : found->second;
}

// --- Small objects the engine would own -----------------------------------------------------

struct FakeFilter {
    void** vtable;
};

void filter_destroy(FakeFilter*, int) {}
void filter_clear(FakeFilter*) {}
FakeFilter* filter_clone(FakeFilter* self, void*) { return self; }
bool filter_test(FakeFilter*, void*) { return g.engine_filter_allows; }

void* g_filter_vtable[4] = {
    reinterpret_cast<void*>(&filter_destroy),
    reinterpret_cast<void*>(&filter_clear),
    reinterpret_cast<void*>(&filter_clone),
    reinterpret_cast<void*>(&filter_test),
};

struct FakeComponent {
    void** vtable;
};

// The owner a component reports through its vtable: the pointer at +840.
std::uint64_t component_owner(void* self) {
    ++g.owner_reads;
    return at<std::uint64_t>(self, 840);
}

void* g_component_vtable[1] = {reinterpret_cast<void*>(&component_owner)};

// A ship answers the class queries `x4n::entity` makes through its vtable: it is a Ship and derives
// from nothing else the bed asks about.
std::uint32_t ship_class_id(const void*) { return static_cast<std::uint32_t>(x4n::GameClass::Ship); }
bool ship_is_a(const void*, std::uint32_t cls) { return cls == static_cast<std::uint32_t>(x4n::GameClass::Ship); }

void* g_ship_vtable[3] = {
    reinterpret_cast<void*>(&component_owner),
    reinterpret_cast<void*>(&ship_class_id),
    reinterpret_cast<void*>(&ship_is_a),
};

std::string g_folder;

}  // namespace

Engine& engine() { return g; }

void boot(const std::string& folder_path) {
    g = Engine{};
    g_folder = folder_path;

    g.offsets = X4GameOffsets{};
    // The SDK's own constants (x4_manual_types.h:648-650): the bed must hand the mod the layout the
    // game has, or every assertion about a record's fields is about the wrong byte.
    g.offsets.blacklist_record_type = 0x08;
    g.offsets.blacklist_record_filter = 0x70;
    g.offsets.blacklist_record_name = 0x10;
    g.offsets.blacklist_registry = &g_registry_slot;
    g.offsets.order_id_map = &g_order_id_map_slot;
    g.offsets.controllable_order_queue = 0x4B0;
    g.offsets.controllable_order_default = 0x500;
    g.offsets.controllable_order_current_index = 0x4E0;
    g.offsets.order_definition = 0x10;
    g.offsets.order_param_values = 0x58;
    g.offsets.trade_seller = 0x10;
    g.offsets.trade_buyer = 0x18;
    g.offsets.vtable_get_faction_id = 0;
    g.offsets.vtable_get_class_id = 1;
    g.offsets.vtable_is_derived_class = 2;
    g.offsets.component_registry = &g_component_registry;
    g.offsets.script_action_command_id = 0x08;
    g.offsets.xml_node_size = 0xF8;
    g.offsets.xml_node_element = 0x0C;
    g.offsets.xml_node_attributes = 0x18;
    g.offsets.xml_node_children = 0xC8;
    g.offsets.xml_node_flags = 0x00;
    g.offsets.xml_node_source_line = 0x02;
    g.offsets.xml_node_source_column = 0x04;
    g.offsets.xml_node_prefix = 0x08;
    g.offsets.xml_node_text = 0x10;
    g.offsets.xml_node_attribute_capacity = 0xB8;
    g.offsets.xml_node_attribute_count = 0xC0;
    g.offsets.xml_node_child_capacity = 0xE8;
    g.offsets.xml_node_child_count = 0xF0;
    g.offsets.xml_attribute_size = 0x28;
    g.offsets.xml_attribute_id = 0x00;
    g.offsets.xml_attribute_value = 0x08;

    g.game.BlacklistTypeFromName = &game_type_from_name;
    g.game.BlacklistRecordById = &game_record_by_id;
    g.game.BlacklistFilterBuild = &game_filter_build;
    g.game.BlacklistFetchForControllable = &game_fetch;
    g.game.FindOfferAction = &game_find_offer;
    g.game.GameAlloc = &game_alloc;
    g.game.XmlNode_SetAttribute = &game_set_attribute;
    g.game.XmlNodeVector_PushBack = &game_push_back;
    g.game.BlacklistData_Export = &original_list_export;
    g.game.BlacklistData_Import = &original_list_import;
    g.game.AutoIDMap_Lookup = &game_resolve_handle;
    g.game.ComponentRegistry_Find = &game_find_component;

    g.registry = g_registry_slot;

    // The fake image: enough room for the faction registry's RVA slot, plus a registry struct and
    // its tree nodes. `exe_base` points at it, which is what the SDK's lookup resolves against.
    g_image.assign(0x06D00000, 0);  // covers the faction registry RVA (0x06CF15F8)
    g_registry = new std::uint8_t[0x40]();
    g_faction_root = nullptr;
    at<void*>(g_image.data(), 0x06CF15F8) = g_registry;
    g.offsets.faction_registry = g_image.data() + 0x06CF15F8;
    g.offsets.object_owner_faction_ptr = 840;

    g_savexml_namespace.assign(X4_SAVEXML_OFFSET_ENTRY_COUNT + sizeof(std::size_t), 0);
    x4n::SaveXml::attach(&game_format_xml_name, &game_lookup_name);  // as the core does once both are hooked
    g_savexml_names.clear();
    g_savexml_names.reserve(256);
    add_game_save_name("blacklist", kSaveBlacklist);
    add_game_save_name("factions", kSaveFactions);
    add_game_save_name("id", kSaveId);

    g.api.api_version = X4NATIVE_API_VERSION;
    g.api.subscribe = &on_subscribe;
    g.api.unsubscribe = &on_unsubscribe;
    g.api.raise_event = &on_raise;
    g.api.raise_lua_event = &on_raise_lua;
    g.api.register_lua_bridge = &on_register_bridge;
    g.api.log = &on_log;
    g.api.get_game_version = &on_game_version;
    g.api.get_x4native_version = &on_native_version;
    g.api.extension_path = g_folder.c_str();
    g.api.game = &g.game;
    g.api.get_game_function = &on_get_function;
    g.api.exe_base = 0;
    g.api.hook_before = &on_hook_before;
    g.api.hook_after = &on_hook_after;
    g.api.unhook = &on_unhook;
    g.api._ensure_detour = &on_ensure_detour;
    g.api.savexml_register = &on_savexml_register;
    g.api._run_before_hooks = &on_run_before;
    g.api._run_after_hooks = &on_run_after;
    g.api.resolve_internal = &on_resolve_internal;
    g.api.offsets = &g.offsets;
    g.api._ext_id = "action_based_trade_restriction";
    g.api._ext_display_name = "Action Based Trade Restriction";
}

void run_before(const char* function, X4HookContext& context) {
    for (const HookEntry& entry : g.hooks) {
        if (entry.after || entry.function != function) continue;
        context.userdata = entry.userdata;  // the core sets this per hook before dispatching
        entry.callback(&context);
    }
}

void run_after(const char* function, X4HookContext& context) {
    for (const HookEntry& entry : g.hooks) {
        if (!entry.after || entry.function != function) continue;
        context.userdata = entry.userdata;  // the core sets this per hook before dispatching
        entry.callback(&context);
    }
}

void raise(const char* name, void* data) {
    const std::string wanted = name ? name : "";
    std::vector<Engine::Subscription> snapshot = g.subscriptions;
    for (const Engine::Subscription& subscription : snapshot) {
        if (subscription.name == wanted) subscription.callback(name, data, subscription.userdata);
    }
}

bool logged(const std::string& fragment) {
    for (const LogLine& line : g.log) {
        if (line.text.find(fragment) != std::string::npos) return true;
    }
    return false;
}

std::string last_lua(const char* name) {
    const std::string wanted = name ? name : "";
    for (auto it = g.lua.rbegin(); it != g.lua.rend(); ++it) {
        if (it->name == wanted) return it->param;
    }
    return {};
}

void* make_record(int type, const char* name, void* engine_filter) {
    void* record = VirtualAlloc(nullptr, 0x400, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    std::memset(record, 0, 0x400);

    at<int>(record, g.offsets.blacklist_record_type) = type;
    at<void*>(record, g.offsets.blacklist_record_filter) = engine_filter;
    // The game and the mod are both MSVC C++: the record's name is a std::string.
    new (&at<std::string>(record, g.offsets.blacklist_record_name)) std::string(name);

    g.records.push_back(record);
    return record;
}

void* make_partner() { return make_partner(nullptr); }

void* make_partner(void* owner) {
    void* partner = new std::uint8_t[0x400]();
    at<void**>(partner, 0) = g_component_vtable;
    at<void*>(partner, 840) = owner;
    return partner;
}

void* add_faction(const char* id) {
    if (!id || !*id) return nullptr;
    const std::uint64_t key = x4n::faction::detail::fnv1_id(id);

    auto* node = new std::uint8_t[0x100]();
    at<std::uint64_t>(node, 0x20) = key;

    if (!g_faction_root) {
        g_faction_root = node;
    } else {
        auto* current = g_faction_root;
        while (true) {
            const std::uint64_t current_key = at<std::uint64_t>(current, 0x20);
            if (key < current_key) {
                auto* left = at<std::uint8_t*>(current, 0x08);
                if (!left) {
                    at<std::uint8_t*>(current, 0x08) = node;
                    break;
                }
                current = left;
            } else {
                auto* right = at<std::uint8_t*>(current, 0x10);
                if (!right) {
                    at<std::uint8_t*>(current, 0x10) = node;
                    break;
                }
                current = right;
            }
        }
    }

    at<void*>(g_registry, 0x18) = g_faction_root;  // registry+24 = root
    return node + 0x30;                                            // FactionClass inline at node+0x30
}

void* make_engine_filter() {
    auto* filter = new FakeFilter{};
    filter->vtable = g_filter_vtable;
    return filter;
}

int record_count() { return static_cast<int>(g.records.size()); }

void rebuild_all_lists() {
    auto* build = reinterpret_cast<void* (*)(void*)>(detour_for("BlacklistFilterBuild"));
    for (void* record : g.records) {
        if (record && build) build(record);
    }
}

int subscription_count() { return static_cast<int>(g.subscriptions.size()); }

void free_record(void* record) {
    for (void*& known : g.records) {
        if (known == record) known = nullptr;
    }
    // Released, not merely forgotten: the next access to it faults, which is what the game does when a
    // load replaces the list registry.
    VirtualFree(record, 0, MEM_RELEASE);
}

// The game's removal of a list (0x140371580): the check in the slot is destroyed through its vtable slot 0
// with argument 1, the slot is cleared, and the registry no longer knows the id.
void forget_record(void* record) {
    void*& slot = at<void*>(record, g.offsets.blacklist_record_filter);
    if (void* check = slot) {
        slot = nullptr;
        void** vtable = at<void**>(check, 0);
        reinterpret_cast<void (*)(void*, int)>(vtable[0])(check, 1);
    }
    for (void*& known : g.records) {
        if (known == record) known = nullptr;
    }
}

void* detour_for(const char* function) {
    const auto found = g.detours.find(function ? function : "");
    return found == g.detours.end() ? nullptr : found->second;
}

void add_game_save_name(const char* name, std::int32_t id) {
    auto& count = at<std::size_t>(g_savexml_namespace.data(), X4_SAVEXML_OFFSET_ENTRY_COUNT);
    g_savexml_names.emplace_back(name);  // reserved, so earlier entries' names stay where they are
    std::uint8_t* entry = g_savexml_namespace.data() + X4_SAVEXML_OFFSET_ENTRIES + count * X4_SAVEXML_ENTRY_SIZE;
    at<const char*>(entry, X4_SAVEXML_ENTRY_OFFSET_NAME) = g_savexml_names.back().c_str();
    at<std::int32_t>(entry, X4_SAVEXML_ENTRY_OFFSET_ID) = id;
    ++count;
}

std::int32_t save_number(const char* name) {
    return x4n::SaveXml::lookup_name(name, nullptr);
}

// A node as the list writer builds one: 0xF8 bytes, the attribute and child vectors empty with their
// four inline entries, the element number at +0x0C.
void* make_save_node(std::int32_t element) {
    void* node = game_alloc(0xF8, 0, 0, 0, 0x10);
    at<std::uint16_t>(node, 0x02) = 0xFFFF;
    at<std::int32_t>(node, 0x04) = -1;
    at<std::size_t>(node, 0xB8) = 4;
    at<std::size_t>(node, 0xE8) = 4;
    at<std::uint8_t>(node, 0x00) = 3;
    at<std::int32_t>(node, 0x0C) = element;
    return node;
}

std::vector<void*> save_children(const void* node) {
    const void* children = &at<std::uint8_t>(node, 0xC8);
    const std::size_t capacity = at<std::size_t>(children, 0x20);
    const std::size_t count = at<std::size_t>(children, 0x28);
    void* const* entries = capacity == 4 ? static_cast<void* const*>(children) : at<void* const*>(children, 0);
    return std::vector<void*>(entries, entries + count);
}

std::int32_t save_element(const void* node) {
    return at<std::int32_t>(node, 0x0C);
}

// The attribute vector at +0x18: entries of 0x28 bytes {int32 id; std::string value at +8}, four inline,
// the capacity at +0xB8 and the count at +0xC0.
namespace {
std::uint8_t* attribute_entries(const void* node, std::size_t** count, std::size_t** capacity) {
    void* attributes = const_cast<std::uint8_t*>(&at<std::uint8_t>(node, 0x18));
    *capacity = &at<std::size_t>(attributes, 0xA0);
    *count = &at<std::size_t>(attributes, 0xA8);
    return **capacity == 4 ? static_cast<std::uint8_t*>(attributes) : at<std::uint8_t*>(attributes, 0);
}
}  // namespace

const std::string* save_attribute(const void* node, std::int32_t attribute) {
    std::size_t* count = nullptr;
    std::size_t* capacity = nullptr;
    std::uint8_t* entries = attribute_entries(node, &count, &capacity);
    for (std::size_t i = 0; i < *count; ++i) {
        void* entry = entries + i * 0x28;
        if (at<std::int32_t>(entry, 0) == attribute) return &at<std::string>(entry, 8);
    }
    return nullptr;
}

void set_save_attribute(void* node, std::int32_t attribute, const char* value) {
    if (const std::string* existing = save_attribute(node, attribute)) {
        *const_cast<std::string*>(existing) = value;
        return;
    }
    std::size_t* count = nullptr;
    std::size_t* capacity = nullptr;
    std::uint8_t* entries = attribute_entries(node, &count, &capacity);
    if (*count == *capacity) {
        auto* grown = new std::uint8_t[*capacity * 2 * 0x28]();
        for (std::size_t i = 0; i < *count; ++i) {
            at<std::int32_t>(grown, i * 0x28) = at<std::int32_t>(entries, i * 0x28);
            new (&at<std::string>(grown, i * 0x28 + 8)) std::string(std::move(at<std::string>(entries, i * 0x28 + 8)));
        }
        at<std::uint8_t*>(node, 0x18) = grown;
        *capacity *= 2;
        entries = grown;
    }
    void* entry = entries + *count * 0x28;
    at<std::int32_t>(entry, 0) = attribute;
    new (&at<std::string>(entry, 8)) std::string(value ? value : "");
    ++*count;
}

void append_save_child(void* parent, void* child) {
    void* const item = child;
    game_push_back(&at<std::uint8_t>(parent, 0xC8), &item);
}

bool run_list_export(void* parent) {
    auto* writer = reinterpret_cast<void* (*)(void*, void*)>(detour_for("BlacklistData_Export"));
    if (!writer) return false;
    writer(g.registry, parent);
    return true;
}

bool run_list_import(void* lists) {
    auto* reader = reinterpret_cast<void* (*)(void*, void*)>(detour_for("BlacklistData_Import"));
    if (!reader) return false;
    reader(g.registry, lists);
    return true;
}

void* make_script_action(std::uint32_t command_id) {
    void* action = new std::uint8_t[0x40]();
    at<std::uint32_t>(action, g.offsets.script_action_command_id) = command_id;
    return action;
}

bool run_find_offer(void* action, std::function<void()> search) {
    auto* handler = reinterpret_cast<void* (*)(void*, void*, void*, void*)>(detour_for("FindOfferAction"));
    if (!handler) return false;
    std::function<void()> outer = std::exchange(g.find_offer_search, std::move(search));
    handler(action, nullptr, nullptr, nullptr);
    g.find_offer_search = std::move(outer);
    return true;
}

void* make_trade(void* seller, void* buyer, std::uint64_t handle) {
    void* trade = new std::uint8_t[0x40]();
    static std::uint64_t next_component_id = 0x40000;
    const std::uint64_t seller_id = seller ? ++next_component_id : 0;
    const std::uint64_t buyer_id = buyer ? ++next_component_id : 0;
    if (seller) g_components[seller_id] = seller;
    if (buyer) g_components[buyer_id] = buyer;
    at<std::uint64_t>(trade, g.offsets.trade_seller) = seller_id;
    at<std::uint64_t>(trade, g.offsets.trade_buyer) = buyer_id;
    g_trades[handle] = trade;
    return trade;
}

void* make_order(std::uint64_t handle) {
    void* order = new std::uint8_t[0x80]();
    at<void*>(order, g.offsets.order_definition) = reinterpret_cast<void*>(1);
    // A script value is 16 order in parameter order and its payload is at +8; the first parameter of a
    // trade order is the trade handle.
    at<std::uint64_t>(order, g.offsets.order_param_values + 8) = handle;
    return order;
}

void* make_controllable(void* order) { return make_controllable(std::vector<void*>{order}, 0, nullptr); }

void* make_controllable(const std::vector<void*>& queue, std::size_t current, void* standing) {
    static std::uint64_t next_ship_id = 0x80000;
    void* controllable = new std::uint8_t[0x600]();
    at<void**>(controllable, 0) = g_ship_vtable;
    at<std::uint64_t>(controllable, 0x08) = ++next_ship_id;  // X4EntityBase::id
    g_components[next_ship_id] = controllable;
    void** orders = new void*[queue.size() + 1]();
    for (std::size_t i = 0; i < queue.size(); ++i) orders[i] = queue[i];
    at<void**>(controllable, g.offsets.controllable_order_queue) = orders;
    at<void**>(controllable, g.offsets.controllable_order_queue + 8) = orders + queue.size();
    at<std::size_t>(controllable, g.offsets.controllable_order_current_index) = current;
    at<void*>(controllable, g.offsets.controllable_order_default) = standing;
    return controllable;
}

std::uint64_t id_of(void* component) { return at<std::uint64_t>(component, 0x08); }

}  // namespace bed
