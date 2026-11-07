// A zero list id wraps nothing.
//
// The table key is a list id, and zero is not one. An entry carrying it must be ignored rather than
// resolved to whatever the engine returns for a zero id.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(zero_id) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");

    bed::send("0|sell:10:argon|buy:00:");

    bed::check("a zero list id wraps nothing", bed::filter_slot(f.record) == f.engine_filter);
}
