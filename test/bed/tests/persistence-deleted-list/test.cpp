// An entry for a deleted list applies to nothing.
//
// The player deletes a list that carried a restriction. After the reload the engine no longer knows
// the id, so the entry wraps nothing and the surviving list still follows its own entry.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(deleted_list) {
    bed::Fixture f = bed::start(folder);

    void* second_engine = bed::make_engine_filter();
    void* second_record = bed::make_record(1, "Scoped Buy", second_engine);

    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    bed::send("2|sell:10:argon|buy:10:terran");

    void* saved = bed::save_game();
    bed::forget_record(second_record);
    bed::load_game(saved);

    // The deletion destroyed the list's check and cleared its slot; the reload must not fill it.
    bed::check("an entry for a deleted list applies to nothing", bed::filter_slot(second_record) == nullptr);

    bed::check("the surviving list still follows its own entry",
               bed::sale_allows(bed::filter_slot(f.record), bed::make_partner(f.terran)) == false);
}
