// A check made from inside find_sell_offer or find_buy_offer is judged on that action's side.
//
// The engine runs the action's handler with the action object, whose command id names the search: from
// find_sell_offer the ship buys, from find_buy_offer it sells. The handler's trade search checks the
// candidates before it returns, so the side holds for the handler's call and no longer.

#include "common.h"
#include "scenario.h"

#include <cstdint>

BED_SCENARIO(command_id_names_the_side) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);
    void* terran = bed::make_partner(f.terran);
    void* argon = bed::make_partner(f.argon);

    bool terran_allowed = true, argon_allowed = true;
    auto search = [&] {
        terran_allowed = bed::wrapper_allows(wrapper, terran);
        argon_allowed = bed::wrapper_allows(wrapper, argon);
    };

    const bool hooked = bed::run_find_offer(bed::make_script_action(0x123), search);
    bed::check("the find-offer handler carries the mod's detour", hooked);
    if (!hooked) return;
    bed::check("a check from another command is judged by both sets", !terran_allowed && !argon_allowed);

    bed::run_find_offer(bed::make_script_action(bed::kFindSellOffer), search);
    bed::check("a check from find_sell_offer is a purchase", terran_allowed && !argon_allowed);

    bed::run_find_offer(bed::make_script_action(bed::kFindBuyOffer), search);
    bed::check("a check from find_buy_offer is a sale", !terran_allowed && argon_allowed);
}

BED_SCENARIO(search_side_before_the_order) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);
    void* terran = bed::make_partner(f.terran);
    void* argon = bed::make_partner(f.argon);

    // The ship's order is a sale; the check runs inside find_sell_offer, where the ship buys.
    const std::uint64_t deal = 0x5001;
    void* ship = bed::make_controllable(bed::make_order(deal));
    bed::make_trade(ship, bed::make_controllable(bed::make_order(0)), deal);
    auto fetch = reinterpret_cast<void* (*)(void*, int, int, int)>(bed::detour_for("BlacklistFetchForControllable"));
    if (!fetch) {
        bed::check("the fetch carries the mod's detour", false);
        return;
    }
    bed::engine().fetched_filter = wrapper;

    bool terran_allowed = false, argon_allowed = true;
    const bool hooked = bed::run_find_offer(bed::make_script_action(bed::kFindSellOffer), [&] {
        fetch(ship, 1, 0, 0);
        terran_allowed = bed::wrapper_allows(wrapper, terran);
        argon_allowed = bed::wrapper_allows(wrapper, argon);
    });
    bed::check("the find-offer handler carries the mod's detour", hooked);
    bed::check("a check inside find_sell_offer is a purchase whatever the fetched ship's order says",
               terran_allowed && !argon_allowed);
}

BED_SCENARIO(searches_nest) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);
    void* argon = bed::make_partner(f.argon);

    bool inner_allows = true, outer_after_inner_allows = true;
    const bool hooked = bed::run_find_offer(bed::make_script_action(bed::kFindSellOffer), [&] {
        bed::run_find_offer(bed::make_script_action(bed::kFindBuyOffer),
                            [&] { inner_allows = bed::wrapper_allows(wrapper, argon); });
        outer_after_inner_allows = bed::wrapper_allows(wrapper, argon);
    });
    bed::check("the find-offer handler carries the mod's detour", hooked);
    bed::check("a find_buy_offer inside a find_sell_offer is a sale", inner_allows);
    bed::check("once the inner search returns, the outer search is a purchase again", !outer_after_inner_allows);
}
