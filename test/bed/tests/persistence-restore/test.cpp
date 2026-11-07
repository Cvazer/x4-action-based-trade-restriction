// The save restores every list after a load.
//
// Each list's sets travel in its own element of the save. A load reads them back and wraps each list,
// and each list then answers by its own sets, so the restore is not one list's settings applied to all.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(save_round_trip) {
    bed::Fixture f = bed::start(folder);

    void* second_engine = bed::make_engine_filter();
    void* second_record = bed::make_record(1, "Scoped Buy", second_engine);

    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    bed::send("2|sell:10:argon|buy:10:terran");

    void* saved = bed::save_game();

    void* rebuilt = bed::make_engine_filter();
    bed::at<void*>(f.record, bed::engine().offsets.blacklist_record_filter) = rebuilt;

    bed::run_list_import(saved);
    bed::check("after a reload nothing is wrapped until the table arrives",
               bed::filter_slot(f.record) == rebuilt);

    bed::raise("on_game_loaded");
    bed::check("the table restores both lists after a reload",
               bed::filter_slot(f.record) != rebuilt &&
                   bed::filter_slot(second_record) != second_engine);

    void* first = bed::filter_slot(f.record);
    void* second = bed::filter_slot(second_record);
    void* terran = bed::make_partner(f.terran);
    void* argon = bed::make_partner(f.argon);

    bed::check("the first list refuses terran on a sale after the reload",
               bed::sale_allows(first, terran) == false);
    bed::check("the first list allows argon on a sale after the reload", bed::sale_allows(first, argon) == true);
    bed::check("the second list refuses argon on a sale after the reload",
               bed::sale_allows(second, argon) == false);
    bed::check("the second list allows terran on a sale after the reload",
               bed::sale_allows(second, terran) == true);
}
