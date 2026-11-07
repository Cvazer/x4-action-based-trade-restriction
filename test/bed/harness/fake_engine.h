// A stand-in for the game, so the mod's own code can be called the way the game calls it.
//
// Nothing here is the mod: it is the engine's side of every call the extension makes - the API table,
// the game function table, the offsets struct, the hook dispatcher, the list registry, a record with
// its evaluated filter, and a partner with an owner. The bed drives these; the extension runs unmodified.
//
// Every fact this fake asserts about the engine is an assumption written down here, in one place, so a
// wrong assumption is visible instead of silently agreeing with the mod.

#pragma once

#include <x4native.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace bed {

// A field of an engine object, by its byte offset.
template <typename T>
T& at(void* base, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + offset);
}

template <typename T>
const T& at(const void* base, std::size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const std::uint8_t*>(base) + offset);
}

struct LogLine {
    int level;
    std::string text;
};

struct LuaCall {
    std::string name;
    std::string param;
};

struct HookEntry {
    int id;
    std::string function;
    bool after;
    X4HookCallback callback;
    void* userdata;
};

struct Engine {
    X4NativeAPI api{};
    X4GameOffsets offsets{};
    X4GameFunctions game{};

    std::vector<LogLine> log;
    std::vector<LuaCall> lua;
    std::vector<HookEntry> hooks;

    struct Subscription {
        int id;
        std::string name;
        X4NativeEventCallback callback;
        void* userdata;
    };
    std::vector<Subscription> subscriptions;

    std::vector<void*> records;  // the record buffers this engine knows about
    void* registry = nullptr;    // the value the registry slot holds
    int next_id = 1;

    // The detours the framework installed, by game function name. Calling one is the engine calling
    // the game function: before hooks, original, after hooks.
    std::map<std::string, void*> detours;

    // What the engine's list fetch answers for a controllable.
    void* fetched_filter = nullptr;

    // The trade search the find-offer handler runs between its entry and its return: the checks of
    // candidate partners a find_sell_offer or find_buy_offer makes.
    std::function<void()> find_offer_search;

    // Whether the game has built its SaveXML namespace, which X4Native reads when names are registered.
    bool savexml_namespace_ready = true;

    // Game functions the framework cannot detour this load: a hook on one of them does not attach.
    std::vector<std::string> detours_refused;

    // What the engine answers when asked, stated once so the bed can change it per scenario.
    int list_type_from_name = 1;   // every name resolves to this type index
    bool engine_filter_allows = true;

    // How many times the mod has read a partner's owner. The per-check cost of a restriction.
    int owner_reads = 0;
};

Engine& engine();

// A faction in the fake registry. Returns the value the engine hands out for it (the `FactionClass*`
// that `x4n::faction::find_class_by_id` resolves), or null if the id is empty.
void* add_faction(const char* id);

// Start the engine and give the mod its API. Writes nothing; the caller owns the folder.
void boot(const std::string& folder);

// The engine calls a function the mod hooked: build the context the detour would build.
void run_before(const char* function, X4HookContext& context);
void run_after(const char* function, X4HookContext& context);

// The detour the framework installed for a game function, or null if nothing hooked it.
void* detour_for(const char* function);

// A name in the game's own SaveXML table, under the number the game gives it.
void add_game_save_name(const char* name, std::int32_t id);

// The game's numbers for the names its list writer uses (finding 03.7), which the fake engine's table
// carries from boot.
constexpr std::int32_t kSaveBlacklist = 0x67;
constexpr std::int32_t kSaveId = 0x1FE;
constexpr std::int32_t kSaveFactions = 0x190;

// The number the save gives a name this session: the game's, or the one X4Native added; 0 for neither.
std::int32_t save_number(const char* name);

// The save's XML as the game holds it: a node built as the list writer builds one, its children, its
// element number and its attributes.
void* make_save_node(std::int32_t element);
std::vector<void*> save_children(const void* node);
std::int32_t save_element(const void* node);
const std::string* save_attribute(const void* node, std::int32_t attribute);
void set_save_attribute(void* node, std::int32_t attribute, const char* value);
void append_save_child(void* parent, void* child);

// The engine writes the list registry into a save under `parent`, one <blacklist> per list with its id,
// through the mod's detour; and reads it back from `lists`. False when nothing hooked the function.
bool run_list_export(void* parent);
bool run_list_import(void* lists);

// The command ids the find-offer handler branches on, and a script action carrying one.
constexpr std::uint32_t kFindSellOffer = 0x7B0;  // the searching ship buys
constexpr std::uint32_t kFindBuyOffer = 0x74E;   // the searching ship sells
void* make_script_action(std::uint32_t command_id);

// The engine runs a find-offer action: through the mod's detour when there is one, the handler running
// `search` before it returns. False when nothing hooked the handler.
bool run_find_offer(void* action, std::function<void()> search);

// The order path: a controllable whose current order carries a trade handle, the trade that handle
// resolves to, and its seller and buyer. Enough for `seh_operation_from_order` to read.
void* make_trade(void* seller, void* buyer, std::uint64_t handle);
void* make_order(std::uint64_t handle);
void* make_controllable(void* order);

// A ship holding `queue` in its order queue, `current` as the queue index and `standing` as its
// Default order, which sits outside the queue. It is registered under its UniverseID and reports
// itself a Ship, so `x4n::ship::Ship` resolves it.
void* make_controllable(const std::vector<void*>& queue, std::size_t current, void* standing);

// The UniverseID a component made by the bed is registered under.
std::uint64_t id_of(void* component);

// The engine removing a list the player deleted, as the game does: the list's check is destroyed through
// its vtable and its slot cleared, and the id resolves to nothing from now on.
void forget_record(void* record);

// How many lists the engine holds, which is also the id of the last one.
int record_count();

// The engine rebuilds the check of every list its registry holds, through the mod's detour.
void rebuild_all_lists();

// The engine freeing a list's record: the pointer is dangling from now on.
void free_record(void* record);

// How many event handlers the mod has registered.
int subscription_count();

// The engine raises an event into the mod (on_game_loaded, a Lua->C++ bridge, ...).
void raise(const char* name, void* data = nullptr);

bool logged(const std::string& fragment);

// The parameter of the last Lua event of that name, which is what the mod last sent the UI.
std::string last_lua(const char* name);

// A list record the engine owns: raw bytes at the offsets the SDK declares, an evaluated filter, a
// type and a name. Returns the buffer; the caller keeps it alive for the scenario.
void* make_record(int type, const char* name, void* engine_filter);

// A partner the engine offers: a component whose vtable reports its owner.
void* make_partner();

// A partner owned by `owner` (a value from `add_faction`). The engine reports it through the field
// at `object_owner_faction_ptr` and through the vtable slot as well.
void* make_partner(void* owner);

// A filter object the engine built for a record: four slots, the third answering the owner test.
void* make_engine_filter();

}  // namespace bed
