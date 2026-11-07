// Extension lifecycle, hook registration, UI message handling and save/load coordination.

#include "domain/rule_table.h"
#include "domain/rules.h"
#include "game/blacklist.h"
#include "game/component.h"
#include "game/xml_node.h"
#include "persistence/save_element.h"
#include "runtime/operation.h"
#include "runtime/wrapper.h"

#include <x4native.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace action_based_trade_restriction;

RuleTable g_rules;
std::int32_t g_trade_type = -1;
bool g_runtime_hooked = false;
std::optional<save::Names> g_names;
bool g_imported = false;

std::vector<int> g_hooks;
std::array<int, 3> g_events = {-1, -1, -1};

// List ID preserved across `BlacklistFilterBuild` when rebuilding an already-wrapped list.
thread_local std::int32_t t_rebuilding_list_id = 0;

void record_hook(const char* name, int id) {
    if (id < 0) {
        x4n::log::warn("hook {}: did not attach; what it carries is left out", name);
        return;
    }
    g_hooks.push_back(id);
}

void report_to_ui() {
    if (game::lists::available()) {
        g_rules.prune(
            [](std::int32_t id) { return game::lists::record(id).has_value(); }
        );
    }
    x4n::raise_lua("action_based_trade_restriction.sets", g_rules.encode().c_str());
}

void apply_rule_to_record(Rule rule, game::ListRecord record) {
    for (const std::string& faction : rule.resolve_owners(game::faction_owner)) {
        x4n::log::error(
            "sets: faction '{}' on list {} did not resolve; that set is switched off",
            faction,
            rule.id
        );
    }
    if (!rule.is_active()) {
        g_rules.erase(rule.id);
        wrapper::unwrap(record);
        return;
    }

    rule.name = record.name();
    g_rules.store(rule);

    if (wrapper::wrap(record, rule.id, g_trade_type)) {
        x4n::log::info(
            "sets: '{}' now sells {} {}({}), buys {} {}({})",
            rule.name,
            rule.sell.enabled ? "as" : "not",
            rule.sell.whitelist ? "whitelist" : "blacklist",
            rule.sell.owners.size(),
            rule.buy.enabled ? "as" : "not",
            rule.buy.whitelist ? "whitelist" : "blacklist",
            rule.buy.owners.size()
        );
    }
}

void apply_entry(const char* payload) {
    if (!payload || !*payload) return;
    auto parsed = parse_entry(payload);
    if (!parsed) {
        x4n::log::error("sets: '{}' is unreadable: {}", payload, parsed.error());
        return;
    }
    if (const auto record = game::lists::record(parsed->id)) {
        apply_rule_to_record(*parsed, *record);
    } else {
        g_rules.erase(parsed->id);
    }
    report_to_ui();
}

void wrap_imported_lists() {
    if (!game::lists::available()) return;
    std::size_t applied = 0;
    const std::vector<Rule> imported = g_rules.all();
    for (const Rule& rule : imported) {
        const auto record = game::lists::record(rule.id);
        if (!record) {
            x4n::log::warn(
                "sets: list {} is named by the save but the game does not hold it; its sets are dropped",
                rule.id
            );
            g_rules.erase(rule.id);
            continue;
        }
        apply_rule_to_record(rule, *record);
        ++applied;
    }
    x4n::log::info("sets: {} trade list(s) carry a set from the save", applied);
}

void clear_rules() {
    wrapper::unwrap_all();
    g_rules.clear();
    operation::reset();
}

void save_sets(game::XmlNode parent) {
    if (!g_names) return;
    std::size_t written = 0;
    for (game::XmlNode node : parent.children()) {
        if (node.element() != g_names->blacklist.number()) continue;
        const Rule* rule = g_rules.find(save::list_id(node, *g_names));
        if (!rule || !rule->is_active()) continue;
        if (save::write(node, *rule, *g_names)) {
            ++written;
        } else {
            x4n::log::error(
                "save: list {}'s sets could not be written; the game's calls are missing",
                rule->id
            );
        }
    }
    x4n::log::info("save: {} list(s) saved with their sets", written);
}

void load_sets(game::XmlNode lists) {
    clear_rules();
    g_imported = true;
    if (!g_names) return;
    std::size_t read = 0;
    for (game::XmlNode node : lists.children()) {
        if (node.element() != g_names->blacklist.number()) continue;
        auto rule = save::read(node, *g_names);
        if (!rule) {
            x4n::log::error(
                "load: list {}'s sets are unreadable: {}; the list is left as the game has it",
                save::list_id(node, *g_names),
                rule.error()
            );
            continue;
        }
        if (!*rule) continue;
        g_rules.store(**rule);
        ++read;
    }
    x4n::log::info("load: {} list(s) carry sets in the save", read);
}

void install_runtime_hooks() {
    record_hook(
        "BlacklistFetchForControllable",
        x4n::hook::after<&X4GameFunctions::BlacklistFetchForControllable>(
            [](void*& result, void* controllable, int type, int, int) {
                if (result && type == g_trade_type) {
                    const game::Filter filter = wrapper::wrapper_for(game::Filter(result));
                    result = filter.object();
                    operation::record_fetch(game::Controllable(controllable), filter);
                }
            }
        )
    );

    record_hook(
        "BlacklistFilterBuild before",
        x4n::hook::before<&X4GameFunctions::BlacklistFilterBuild>(
            [](x4n::hook::HookControl&, void*& record) {
                t_rebuilding_list_id = wrapper::list_id_of(game::ListRecord(record).filter());
            }
        )
    );

    record_hook(
        "BlacklistFilterBuild",
        x4n::hook::after<&X4GameFunctions::BlacklistFilterBuild>(
            [](void*&, void* record) {
                const std::int32_t id = t_rebuilding_list_id;
                t_rebuilding_list_id = 0;
                if (id && g_rules.find(id)) {
                    wrapper::wrap(game::ListRecord(record), id, g_trade_type);
                }
            }
        )
    );

    record_hook(
        "FindOfferAction before",
        x4n::hook::before<&X4GameFunctions::FindOfferAction>(
            [](x4n::hook::HookControl&, void*& action, void*&, void*&, void*&) {
                operation::enter_search(game::ScriptAction(action));
            }
        )
    );

    record_hook(
        "FindOfferAction after",
        x4n::hook::after<&X4GameFunctions::FindOfferAction>(
            [](void*&, void*, void*, void*, void*) {
                operation::leave_search();
            }
        )
    );
}

void install_save_hooks() {
    if (!g_names) return;
    record_hook(
        "BlacklistData_Export",
        x4n::hook::after<&X4GameFunctions::BlacklistData_Export>(
            [](void*&, void*, void* parent) {
                save_sets(game::XmlNode(parent));
            }
        )
    );
    record_hook(
        "BlacklistData_Import",
        x4n::hook::before<&X4GameFunctions::BlacklistData_Import>(
            [](x4n::hook::HookControl&, void*&, void*& lists) {
                load_sets(game::XmlNode(lists));
            }
        )
    );
}

void attach() {
    if (!x4n::game() || !x4n::detail::offsets()) {
        x4n::log::warn("no game table or offsets; nothing attached this load");
        return;
    }
    g_trade_type = game::lists::trade_type().value_or(-1);
    if (!g_runtime_hooked) {
        install_runtime_hooks();
        g_runtime_hooked = true;
    }
}

void on_game_loaded() {
    x4n::log::info("game world loaded; attaching the native half");
    if (!g_imported) clear_rules();
    g_imported = false;
    attach();
    wrap_imported_lists();
    x4n::log::info(
        "sets: {} rule(s) from the save; a trade list the save does not name is left exactly as the game has it",
        g_rules.size()
    );
    x4n::raise_lua("action_based_trade_restriction.arm", nullptr);
}

}  // namespace

X4N_EXTENSION {
    x4n::log::info("loaded, framework {}", x4n::version());
    wrapper::bind(g_rules);

    g_names = save::register_names();
    if (!g_names) {
        x4n::log::error(
            "save: X4Native cannot add the mod's names to the save; the sets apply this session "
            "but are neither saved nor loaded"
        );
    }
    install_save_hooks();

    g_events[0] = x4n::on("action_based_trade_restriction.set", &apply_entry);
    g_events[1] = x4n::on("action_based_trade_restriction.request", &report_to_ui);
    g_events[2] = x4n::on("on_game_loaded", &on_game_loaded);
}

X4N_SHUTDOWN {
    wrapper::unwrap_all();
    for (int id : g_hooks) {
        x4n::hook::remove(id);
    }
    g_hooks.clear();
    g_runtime_hooked = false;

    for (int& id : g_events) {
        if (id >= 0) x4n::off(id);
        id = -1;
    }

    g_rules.clear();
    operation::reset();
    x4n::log::info("unloaded, wrappers removed, hooks and handlers released");
}
