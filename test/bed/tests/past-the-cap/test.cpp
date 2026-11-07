// A list beyond the old 512-entry cap is still found.
//
// The engine holds more lists than the prototype's fixed buffer allowed. The last one is wrapped
// when its entry arrives: a list is found by its id, with no walk over every list.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(past_the_cap) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");

    for (int i = 0; i < 520; ++i) {
        bed::make_record(1, "Bulk", bed::make_engine_filter());
    }
    const int last_id = bed::record_count();
    void* last = bed::engine().records.back();
    void* last_engine = bed::filter_slot(last);

    const std::string entry = std::to_string(last_id) + "|sell:10:argon|buy:00:";
    bed::send(entry.c_str());

    bed::check("a list past the old cap is still found", bed::filter_slot(last) != last_engine);
}
