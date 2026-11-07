// A trade list's sets in the save: the mod's element inside the list's own <blacklist> element.
//
// A list with a set on gets the element when the game saves, both sides written; a list with no set gets
// none. A load reads the element of every list back under that list's id and wraps those lists, by id,
// once the world is loaded, and no others. An element missing a side or an attribute is refused.

#include "common.h"
#include "scenario.h"

#include <string>

namespace {

// The <blacklist> node the save holds for a list id, or null.
void* list_node(void* saved, int id) {
    for (void* node : bed::save_children(saved)) {
        const std::string* value = bed::save_attribute(node, bed::kSaveId);
        if (bed::save_element(node) == bed::kSaveBlacklist && value && *value == std::to_string(id)) return node;
    }
    return nullptr;
}

// The child of `node` whose element is `name`, or null.
void* child(void* node, const char* name) {
    if (!node) return nullptr;
    for (void* c : bed::save_children(node)) {
        if (bed::save_element(c) == bed::save_number(name)) return c;
    }
    return nullptr;
}

std::string attribute(void* node, const char* name) {
    const std::string* value = node ? bed::save_attribute(node, bed::save_number(name)) : nullptr;
    return value ? *value : std::string("<none>");
}

}  // namespace

BED_SCENARIO(set_saved_in_its_list) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:11:argon,terran");

    void* saved = bed::save_game();
    void* element = child(list_node(saved, 1), "action_based_trade_restriction");
    bed::check("a list with a set on carries the mod's element in its own <blacklist>", element != nullptr);
    void* sell = child(element, "sell");
    void* buy = child(element, "buy");
    bed::check("both sides are written", sell && buy);
    bed::check("the selling side is written as set",
               attribute(sell, "enabled") == "1" && attribute(sell, "whitelist") == "0" &&
                   attribute(sell, "factions") == "terran");
    bed::check("the buying side is written as set, its factions in the game's form",
               attribute(buy, "enabled") == "1" && attribute(buy, "whitelist") == "1" &&
                   attribute(buy, "factions") == "argon terran");
}

BED_SCENARIO(no_set_no_element) {
    bed::Fixture f = bed::start(folder);
    void* second = bed::make_record(1, "Unrestricted", bed::make_engine_filter());
    (void)second;
    bed::raise("on_game_loaded");
    bed::send("1|sell:00:terran|buy:00:");

    void* saved = bed::save_game();
    bed::check("the writer still writes every list", list_node(saved, 1) && list_node(saved, 2));
    bed::check("a list with no set on gets no element",
               child(list_node(saved, 1), "action_based_trade_restriction") == nullptr);
    bed::check("a list the table does not name gets no element",
               child(list_node(saved, 2), "action_based_trade_restriction") == nullptr);
}

BED_SCENARIO(load_wraps_by_id) {
    bed::Fixture f = bed::start(folder);
    void* second_engine = bed::make_engine_filter();
    void* second = bed::make_record(1, "Unrestricted", second_engine);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* saved = bed::save_game();

    // A fresh world: the game rebuilt the list's filter, and nothing is wrapped.
    void* rebuilt = bed::make_engine_filter();
    bed::at<void*>(f.record, bed::engine().offsets.blacklist_record_filter) =
        rebuilt;
    bed::run_list_import(saved);
    bed::check("nothing is wrapped while the save is being read", bed::filter_slot(f.record) == rebuilt);
    bed::raise("on_game_loaded");

    bed::check("the list the save names is wrapped once the world is loaded", bed::filter_slot(f.record) != rebuilt);
    bed::check("a list the save does not name is left as the game has it", bed::filter_slot(second) == second_engine);
    void* wrapper = bed::filter_slot(f.record);
    bed::check("the loaded list judges a sale by its saved selling set",
               bed::sale_allows(wrapper, bed::make_partner(f.terran)) == false &&
                   bed::sale_allows(wrapper, bed::make_partner(f.argon)) == true);
    bed::check("the loaded list judges a purchase by its saved buying set",
               bed::purchase_allows(wrapper, bed::make_partner(f.argon)) == false);
}

BED_SCENARIO(element_missing_a_part_is_refused) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");

    // A save whose list 1 carries the element with its buying side missing.
    void* saved = bed::make_save_node(0);
    void* list = bed::make_save_node(bed::kSaveBlacklist);
    bed::set_save_attribute(list, bed::kSaveId, "1");
    void* element = bed::make_save_node(bed::save_number("action_based_trade_restriction"));
    void* sell = bed::make_save_node(bed::save_number("sell"));
    bed::set_save_attribute(sell, bed::save_number("enabled"), "1");
    bed::set_save_attribute(sell, bed::save_number("whitelist"), "0");
    bed::set_save_attribute(sell, bed::save_number("factions"), "terran");
    bed::append_save_child(element, sell);
    bed::append_save_child(list, element);
    bed::append_save_child(saved, list);

    bed::load_game(saved);
    bed::check("an element missing a side is refused, and its list left as the game has it",
               bed::filter_slot(f.record) == f.engine_filter && bed::logged("unreadable"));

    // The same side, now present, with one attribute missing.
    void* buy = bed::make_save_node(bed::save_number("buy"));
    bed::set_save_attribute(buy, bed::save_number("enabled"), "1");
    bed::set_save_attribute(buy, bed::save_number("factions"), "argon");
    bed::append_save_child(element, buy);
    bed::load_game(saved);
    bed::check("a side missing an attribute is refused", bed::filter_slot(f.record) == f.engine_filter);

    bed::set_save_attribute(buy, bed::save_number("whitelist"), "0");
    bed::load_game(saved);
    bed::check("the complete element is read", bed::filter_slot(f.record) != f.engine_filter);
}

BED_SCENARIO(new_game_starts_empty) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    bed::raise("on_game_loaded");  // a world loaded without reading a save
    bed::check("a world that read no save carries no sets", bed::filter_slot(f.record) == f.engine_filter);
}
