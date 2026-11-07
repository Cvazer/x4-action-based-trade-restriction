// A set whose faction did not resolve is switched off, and the other still applies.
//
// One set names a faction the engine cannot resolve. That set must stop refusing anyone while the
// set that did resolve keeps working, so a bad entry costs one set rather than the list.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(unresolved_faction) {
    bed::Fixture f = bed::start(folder);

    void* bad_engine = bed::make_engine_filter();
    void* bad_record = bed::make_record(1, "Bad Faction", bad_engine);

    bed::raise("on_game_loaded");
    const std::string entry =
        std::to_string(bed::record_count()) + "|sell:10:nonexistent|buy:10:argon";
    bed::send(entry.c_str());

    void* bad_wrapper = bed::filter_slot(bad_record);
    bed::check("the list is still wrapped for the set that resolved", bad_wrapper != bad_engine);

    bed::check("the set that failed no longer refuses anyone",
               bed::sale_allows(bad_wrapper, bed::make_partner(f.terran)) == true);
    bed::check("the set that resolved still refuses its partner",
               bed::purchase_allows(bad_wrapper, bed::make_partner(f.argon)) == false);
}
