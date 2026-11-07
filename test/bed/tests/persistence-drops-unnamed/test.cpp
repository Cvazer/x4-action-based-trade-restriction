// A load unwraps every list the table no longer names.
//
// Two lists are wrapped, then the save comes back naming one. The other returns to the engine's own
// filter, so the sets a save carries are the whole truth about which lists are restricted.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(load_drops_unnamed) {
    bed::Fixture f = bed::start(folder);

    void* second_engine = bed::make_engine_filter();
    void* second_record = bed::make_record(1, "Scoped Buy", second_engine);

    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    bed::send("2|sell:10:argon|buy:10:terran");

    bed::send("2|sell:00:|buy:00:");  // the player switches list 2's sets off
    bed::load_game(bed::save_game());

    bed::check("a list the table no longer names is unwrapped",
               bed::filter_slot(second_record) == second_engine);
    bed::check("the list it still names is wrapped again",
               bed::filter_slot(f.record) != f.engine_filter);
}
