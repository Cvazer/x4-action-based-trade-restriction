// A check reads the partner once per switched-on restriction, and never when nothing is set.
//
// The fake engine counts how many times the mod asks a component for its owner. A list with both
// restrictions off must not ask at all, and a list with one on must ask exactly once per check,
// whatever the size of that restriction's faction set.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(cost) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");

    const int reads_before = bed::engine().owner_reads;
    void* none_engine = bed::make_engine_filter();
    void* none_record = bed::make_record(1, "No Sets", none_engine);
    const std::string none_entry =
        std::to_string(bed::record_count()) + "|sell:00:|buy:00:";
    bed::send(none_entry.c_str());

    bed::wrapper_allows(bed::filter_slot(none_record), bed::make_partner(f.terran));
    bed::check("nothing set means the partner is never read",
               bed::engine().owner_reads == reads_before);

    void* one_engine = bed::make_engine_filter();
    void* one_record = bed::make_record(1, "One Set", one_engine);
    const std::string one_entry =
        std::to_string(bed::record_count()) + "|sell:10:argon|buy:00:";
    bed::send(one_entry.c_str());

    const int reads_before_one = bed::engine().owner_reads;
    bed::wrapper_allows(bed::filter_slot(one_record), bed::make_partner(f.terran));
    bed::check("one restriction costs one read of the partner",
               bed::engine().owner_reads == reads_before_one + 1);
}
