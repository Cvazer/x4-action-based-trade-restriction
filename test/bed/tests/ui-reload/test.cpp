// A UI reload keeps every list's sets.
//
// The reload unloads the extension and loads it again in the same world, with no save read in between.
// The sets cross it in X4Native's stash and wrap the same lists, and the next save writes them. A world
// loaded after the reload takes its sets from its own save, or starts with none.

#include "common.h"
#include "events.h"
#include "scenario.h"

namespace {

void reload_ui() {
    x4native_shutdown();
    x4native_init(&bed::engine().api);
}

}  // namespace

BED_SCENARIO(ui_reload_keeps_the_sets) {
    bed::Fixture f = bed::start(folder);
    void* second_engine = bed::make_engine_filter();
    void* second_record = bed::make_record(1, "Scoped Buy", second_engine);

    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    bed::send("2|sell:00:|buy:11:argon");

    reload_ui();
    bed::check("the reload hands the lists back to the engine",
               bed::filter_slot(f.record) == f.engine_filter && bed::filter_slot(second_record) == second_engine);

    bed::raise("on_game_loaded");
    bed::check("both lists are wrapped again after the reload",
               bed::filter_slot(f.record) != f.engine_filter && bed::filter_slot(second_record) != second_engine);
    bed::check("the reload is logged with the lists it kept", bed::logged("reload: 2 list(s) carry sets"));

    void* terran = bed::make_partner(f.terran);
    void* argon = bed::make_partner(f.argon);
    bed::check("the first list still refuses terran on a sale",
               bed::sale_allows(bed::filter_slot(f.record), terran) == false &&
                   bed::sale_allows(bed::filter_slot(f.record), argon) == true);
    bed::check("the second list still buys from argon only",
               bed::purchase_allows(bed::filter_slot(second_record), argon) == true &&
                   bed::purchase_allows(bed::filter_slot(second_record), terran) == false);

    bed::raise(bed::events::request);
    bed::check("the editor is answered with the kept sets",
               bed::notation(bed::last_lua(bed::events::sets)) == "1|sell:10:terran|buy:00:;2|sell:00:|buy:11:argon");

    void* saved = bed::save_game();
    bed::check("the next save writes both lists", bed::logged("save: 2 list(s) saved with their sets"));

    bed::send("1|sell:00:|buy:00:");
    bed::send("2|sell:00:|buy:00:");
    bed::load_game(saved);
    bed::check("that save restores both lists",
               bed::filter_slot(f.record) != f.engine_filter && bed::filter_slot(second_record) != second_engine);
}

BED_SCENARIO(ui_reload_twice_keeps_the_sets) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    reload_ui();
    reload_ui();
    bed::raise("on_game_loaded");
    bed::check("a reload with no world loaded in between keeps the sets",
               bed::filter_slot(f.record) != f.engine_filter);
}

BED_SCENARIO(new_world_after_ui_reload_starts_empty) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    reload_ui();
    bed::engine().player_id = 2;
    bed::raise("on_game_loaded");
    bed::check("a world loaded without a save does not take the last world's sets",
               bed::filter_slot(f.record) == f.engine_filter);

    bed::engine().player_id = 1;
    reload_ui();
    bed::raise("on_game_loaded");
    bed::check("the stash is spent once a world is loaded", bed::filter_slot(f.record) == f.engine_filter);
}

BED_SCENARIO(save_loaded_after_ui_reload_wins) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    void* saved = bed::save_game();
    bed::send("1|sell:10:argon|buy:00:");

    reload_ui();
    bed::engine().player_id = 2;
    bed::load_game(saved);

    void* wrapper = bed::filter_slot(f.record);
    bed::check("the list is judged by the save's sets, not the ones kept across the reload",
               wrapper != f.engine_filter && bed::sale_allows(wrapper, bed::make_partner(f.terran)) == false &&
                   bed::sale_allows(wrapper, bed::make_partner(f.argon)) == true);
}

BED_SCENARIO(ui_reload_between_a_save_and_its_world) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");
    void* saved = bed::save_game();
    bed::send("1|sell:00:|buy:00:");

    bed::run_list_import(saved);
    reload_ui();
    bed::engine().player_id = 2;
    bed::raise("on_game_loaded");
    bed::check("sets read from a save cross a reload into that save's world",
               bed::filter_slot(f.record) != f.engine_filter &&
                   bed::sale_allows(bed::filter_slot(f.record), bed::make_partner(f.terran)) == false);
}
