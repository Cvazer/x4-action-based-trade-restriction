// Unloading puts the engine's own filter back.
//
// Every wrapped list is restored to the filter the engine built, so a session that unloads the mod
// behaves as the game alone does and leaves no wrapper behind.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(unload) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");

    void* rebuilt = bed::make_engine_filter();
    bed::at<void*>(f.record, bed::engine().offsets.blacklist_record_filter) = rebuilt;
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    bed::check("a list is wrapped before the unload", bed::filter_slot(f.record) != rebuilt);

    x4native_shutdown();
    bed::check("the record's filter is the engine's again", bed::filter_slot(f.record) == rebuilt);
}
