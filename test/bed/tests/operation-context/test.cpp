// A sale and a purchase are judged by their own side.
//
// One list refuses terran on its selling set and argon on its buying set. A find_buy_offer search, where
// the ship sells, must refuse terran and allow argon; a find_sell_offer search, where it buys, the
// reverse. A check that mixes the sides fails every one of these.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(sale_and_purchase) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");

    void* wrapper = bed::filter_slot(f.record);
    void* terran = bed::make_partner(f.terran);
    void* argon = bed::make_partner(f.argon);

    bool terran_allowed = true, argon_allowed = false;
    auto search = [&] {
        terran_allowed = bed::wrapper_allows(wrapper, terran);
        argon_allowed = bed::wrapper_allows(wrapper, argon);
    };

    if (!bed::run_find_offer(bed::make_script_action(bed::kFindBuyOffer), search)) {
        bed::check("the find-offer handler carries the mod's detour", false);
        return;
    }
    bed::check("a sale refuses the partner the selling restriction refuses", !terran_allowed);
    bed::check("a sale allows the partner the buying restriction refuses", argon_allowed);

    bed::run_find_offer(bed::make_script_action(bed::kFindSellOffer), search);
    bed::check("a purchase refuses the partner the buying restriction refuses", !argon_allowed);
    bed::check("a purchase allows the partner the selling restriction refuses", terran_allowed);
}
