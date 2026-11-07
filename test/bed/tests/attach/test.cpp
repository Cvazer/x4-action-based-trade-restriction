// What a game load attaches, and what it does when a piece is missing.
//
// A hook that does not attach leaves the others working. Without a usable trade list type no list is
// wrapped. A load arms the UI, and an unload releases every handler the mod registered.

#include "common.h"
#include "events.h"
#include "scenario.h"

BED_SCENARIO(hook_that_does_not_attach) {
    bed::Fixture f = bed::start(folder);
    bed::engine().detours_refused = {"BlacklistFilterBuild"};
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");

    bed::check("a list is wrapped when one hook does not attach",
               bed::filter_slot(f.record) != f.engine_filter);
    bed::check("the hook that did not attach carries no detour",
               bed::detour_for("BlacklistFilterBuild") == nullptr);

    void* wrapper = bed::filter_slot(f.record);
    bed::check("the hooks that attached still judge their side",
               bed::purchase_allows(wrapper, bed::make_partner(f.argon)) == false &&
                   bed::purchase_allows(wrapper, bed::make_partner(f.terran)) == true);
}

BED_SCENARIO(unusable_trade_type) {
    bed::Fixture f = bed::start(folder);
    bed::engine().list_type_from_name = 5;
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    bed::check("an unusable trade type index wraps nothing",
               bed::filter_slot(f.record) == f.engine_filter);
}

BED_SCENARIO(missing_type_resolver) {
    bed::Fixture f = bed::start(folder);
    bed::engine().game.BlacklistTypeFromName = nullptr;
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:00:");

    bed::check("without the type resolver no list is wrapped",
               bed::filter_slot(f.record) == f.engine_filter);
}

BED_SCENARIO(load_arms_the_ui) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");

    bool armed = false;
    for (const bed::LuaCall& call : bed::engine().lua) {
        if (call.name == bed::events::arm) armed = true;
    }
    bed::check("a game load sends the UI its arm event", armed);
}

BED_SCENARIO(unload_releases_handlers) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::check("the mod registers its handlers on load", bed::subscription_count() > 0);

    x4native_shutdown();
    bed::check("an unload releases every handler", bed::subscription_count() == 0);
}
