// A load survives a wrapped record the engine freed.
//
// The engine frees a list's record while the mod still holds a wrapper that points at it. The next
// load must not write through the freed pointer, and the lists that still exist must be wrapped.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(freed_record) {
    bed::Fixture f = bed::start(folder);

    void* doomed = bed::make_record(1, "Doomed", bed::make_engine_filter());
    const std::string entry =
        std::to_string(bed::record_count()) + "|sell:10:argon|buy:00:";

    bed::raise("on_game_loaded");
    bed::send(entry.c_str());
    bed::check("the record that is about to be freed is wrapped",
               bed::filter_slot(doomed) != nullptr);

    bed::send("1|sell:10:terran|buy:10:argon");
    void* saved = bed::save_game();
    bed::forget_record(doomed);
    bed::free_record(doomed);
    bed::load_game(saved);

    bed::check("a load survives a record the engine freed",
               bed::filter_slot(f.record) != f.engine_filter);
}
