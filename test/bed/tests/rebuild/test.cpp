// A filter the engine rebuilds is wrapped again.
//
// The engine rebuilds a list's evaluated filter when the list changes, discarding the old one through
// its vtable, and it can rebuild every list at once. The filter-build hook must wrap the new filter for
// each list that was wrapped, under that list's own rule, or the restriction stops applying until the
// next load; a list that was not wrapped stays as the game built it.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(filter_rebuild) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void** wrapper_vtable = bed::at<void**>(bed::filter_slot(f.record), 0);

    bed::check("the filter-build detour runs without a fault", bed::build_safely(f.record));
    void* after = bed::filter_slot(f.record);
    bed::check("a rebuilt filter for a named list is wrapped",
               after != nullptr && bed::at<void**>(after, 0) == wrapper_vtable &&
                   bed::sale_allows(after, bed::make_partner(f.terran)) == false);
}

BED_SCENARIO(rebuild_of_every_list) {
    bed::Fixture f = bed::start(folder);
    void* second = bed::make_record(1, "Scoped Buy", bed::make_engine_filter());
    void* plain = bed::make_record(1, "Unrestricted", bed::make_engine_filter());
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    bed::send("2|sell:10:argon|buy:00:");

    bed::rebuild_all_lists();

    void* first = bed::filter_slot(f.record);
    void* again = bed::filter_slot(second);
    bed::check("every wrapped list is wrapped again with its own rule",
               bed::sale_allows(first, bed::make_partner(f.terran)) == false &&
                   bed::sale_allows(first, bed::make_partner(f.argon)) == true &&
                   bed::sale_allows(again, bed::make_partner(f.argon)) == false &&
                   bed::sale_allows(again, bed::make_partner(f.terran)) == true);
    void* plain_slot = bed::filter_slot(plain);
    bed::check("a list that was not wrapped is as the game built it",
               plain_slot != nullptr && bed::sale_allows(plain_slot, bed::make_partner(f.terran)) == true &&
                   bed::sale_allows(plain_slot, bed::make_partner(f.argon)) == true);
    bed::engine().engine_filter_allows = false;
    bed::check("that list answers with the game's own verdict alone",
               bed::wrapper_allows(plain_slot, bed::make_partner(f.terran)) == false);
    bed::engine().engine_filter_allows = true;
}
