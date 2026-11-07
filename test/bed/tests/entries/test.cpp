// What an entry does to the table the mod holds and reports.
//
// Each entry from the editor or the store replaces that list's rule and the whole table goes back to
// the UI. An entry that cannot be read changes nothing. A list the engine no longer knows leaves the
// table, and a list of another type is never wrapped.

#include "common.h"
#include "events.h"
#include "scenario.h"

#include <string>

namespace {

int reports() {
    int count = 0;
    for (const bed::LuaCall& call : bed::engine().lua) {
        if (call.name == bed::events::sets) ++count;
    }
    return count;
}

}  // namespace

BED_SCENARIO(entry_reports_the_table) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");

    bed::send("1|sell:10:terran|buy:00:");
    bed::check("an entry is answered with the table the mod holds",
               bed::notation(bed::last_lua(bed::events::sets)) == "1|sell:10:terran|buy:00:");

    const int before = reports();
    bed::raise(bed::events::request);
    bed::check("a request is answered with the table", reports() == before + 1 &&
                   bed::notation(bed::last_lua(bed::events::sets)) == "1|sell:10:terran|buy:00:");
}

BED_SCENARIO(unreadable_entry_changes_nothing) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    const int before = reports();
    bed::send_raw(R"({"list":1,"sell":{"enabled":true}})");
    bed::send_raw(R"({"list":1,"sell":{"enabled":1,"whitelist":true,"factions":["argon"]},"buy":{"enabled":false,"whitelist":false,"factions":[]}})");
    bed::send_raw(R"({"list":1,"keep":{"enabled":true,"whitelist":false,"factions":["argon"]}})");
    bed::send_raw(R"({"list":1})");
    bed::send_raw("1|sell:10:terran|buy:00:");
    bed::check("an unreadable entry is not answered", reports() == before);
    bed::check("an unreadable entry is logged", bed::logged("is unreadable"));
    bed::check("an unreadable entry leaves the table as it was",
               bed::notation(bed::last_lua(bed::events::sets)) == "1|sell:10:terran|buy:00:");

    void* wrapper = bed::filter_slot(f.record);
    bed::check("the list still judges a sale by its stored rule",
               bed::sale_allows(wrapper, bed::make_partner(f.terran)) == false &&
                   bed::sale_allows(wrapper, bed::make_partner(f.argon)) == true);
}

BED_SCENARIO(entry_replaces_the_rule) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    bed::send("1|sell:10:argon|buy:00:");

    void* wrapper = bed::filter_slot(f.record);
    bed::check("a replaced entry judges by the new rule",
               bed::sale_allows(wrapper, bed::make_partner(f.terran)) == true &&
                   bed::sale_allows(wrapper, bed::make_partner(f.argon)) == false);
    bed::check("the table holds one entry for the list",
               bed::notation(bed::last_lua(bed::events::sets)) == "1|sell:10:argon|buy:00:");
}

BED_SCENARIO(deleted_list_leaves_the_report) {
    bed::Fixture f = bed::start(folder);
    void* second = bed::make_record(1, "Scoped Buy", bed::make_engine_filter());
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    bed::send("2|sell:00:|buy:10:argon");

    bed::forget_record(second);
    bed::raise(bed::events::request);
    bed::check("a list the engine no longer knows leaves the reported table",
               bed::notation(bed::last_lua(bed::events::sets)) == "1|sell:10:terran|buy:00:");
}

BED_SCENARIO(other_list_type_is_not_wrapped) {
    bed::Fixture f = bed::start(folder);
    void* engine_filter = bed::make_engine_filter();
    void* travel = bed::make_record(0, "Travel", engine_filter);
    bed::raise("on_game_loaded");
    bed::send("2|sell:10:terran|buy:00:");

    bed::check("an entry for a list of another type wraps nothing",
               bed::filter_slot(travel) == engine_filter);
}
