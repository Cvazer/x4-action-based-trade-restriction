// An entry wraps its list, and the engine's own verdict stays underneath.
//
// A list with no entry keeps the engine's filter. Once an entry arrives the list's slot holds the
// mod's wrapper, which consults the engine's filter first and then the two sets, so a partner the
// game refuses is refused whatever the mod thinks.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(load_and_wrap) {
    bed::Fixture f = bed::start(folder);

    bed::raise("on_game_loaded");
    bed::check("a list with no entry is left as the game has it",
               bed::filter_slot(f.record) == f.engine_filter);

    bed::send("1|sell:10:terran|buy:10:argon");
    bed::check("the stored entry wraps its list", bed::filter_slot(f.record) != f.engine_filter);

    void* wrapper = bed::filter_slot(f.record);
    void* partner = bed::make_partner();

    bed::engine().engine_filter_allows = true;
    bed::check("the wrapped filter allows what the game allows",
               bed::wrapper_allows(wrapper, partner));
    bed::engine().engine_filter_allows = false;
    bed::check("the wrapped filter refuses what the game refuses",
               !bed::wrapper_allows(wrapper, partner));
    bed::engine().engine_filter_allows = true;

    bed::check("an unestablished check is judged by the selling restriction",
               !bed::wrapper_allows(wrapper, bed::make_partner(f.terran)));
    bed::check("an unestablished check is judged by the buying restriction",
               !bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
}
BED_SCENARIO(both_sets_off_unwraps) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    bed::check("an entry with a set on wraps its list", bed::filter_slot(f.record) != f.engine_filter);

    bed::send("1|sell:00:terran|buy:00:");
    bed::check("an entry with both sets off leaves its list unwrapped", bed::filter_slot(f.record) == f.engine_filter);
    void* saved = bed::save_game();
    bool element = false;
    for (void* list : bed::save_children(saved)) {
        if (!bed::save_children(list).empty()) element = true;
    }
    bed::check("a list with both sets off gets no element on save", !element);

    bed::send("1|sell:10:terran|buy:00:");
    bed::check("a set switched on again wraps the list", bed::filter_slot(f.record) != f.engine_filter);
}
