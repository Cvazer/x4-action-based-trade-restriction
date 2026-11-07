// Which side of a trade a check is judged on, where no find-offer search says.
//
// One list refuses terran on its selling set and argon on its buying set. A ship executes its queue's
// current order while the queue is not empty, and its Default order only otherwise; an order index
// outside the queue leaves the side unknown, and an unknown side is judged by both sets. A fetch names
// its ship for the one check that follows it. A search's side ends when the search returns.

#include "common.h"
#include "scenario.h"

#include <cstdint>

namespace {

using Fetch = void* (*)(void*, int, int, int);


Fetch fetch() { return reinterpret_cast<Fetch>(bed::detour_for("BlacklistFetchForControllable")); }

}  // namespace

BED_SCENARIO(queued_order_before_default) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);

    // A ship with a standing trade routine, where it sells, executing a queued trade, where it buys.
    const std::uint64_t buying = 0x2001, selling = 0x2002;
    void* ship = bed::make_controllable({bed::make_order(buying)}, 0, bed::make_order(selling));
    bed::make_trade(bed::make_controllable(bed::make_order(0)), ship, buying);
    bed::make_trade(ship, bed::make_controllable(bed::make_order(0)), selling);

    Fetch fetched = fetch();
    bed::engine().fetched_filter = wrapper;
    bed::check("the fetch carries the mod's detour", fetched != nullptr);
    if (!fetched) return;
    fetched(ship, 1, 0, 0);
    bed::check("the queued trade decides the side, not the Default order",
               bed::wrapper_allows(wrapper, bed::make_partner(f.terran)));
    fetched(ship, 1, 0, 0);
    bed::check("the queued trade is a purchase", !bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
}

BED_SCENARIO(fetch_names_one_check) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);

    const std::uint64_t deal = 0x4001;
    void* ship = bed::make_controllable(bed::make_order(deal));
    bed::make_trade(ship, bed::make_controllable(bed::make_order(0)), deal);

    Fetch fetched = fetch();
    bed::engine().fetched_filter = wrapper;
    if (!fetched) {
        bed::check("the fetch carries the mod's detour", false);
        return;
    }
    fetched(ship, 1, 0, 0);
    bed::check("the check after a fetch takes the fetched ship's side, a sale",
               bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
    bed::check("a check with no fetch before it gets no side from the earlier ship, judged by both sets",
               !bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
}

BED_SCENARIO(order_index_outside_the_queue) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);

    const std::uint64_t deal = 0x3001;
    void* ship = bed::make_controllable(bed::make_order(deal));
    bed::make_trade(ship, bed::make_controllable(bed::make_order(0)), deal);
    bed::at<std::size_t>(ship, bed::engine().offsets.controllable_order_current_index) = 5;

    Fetch fetched = fetch();
    bed::engine().fetched_filter = wrapper;
    if (!fetched) {
        bed::check("the fetch carries the mod's detour", false);
        return;
    }
    fetched(ship, 1, 0, 0);
    bed::check("an order index outside the queue leaves the side unknown, judged by both sets",
               !bed::wrapper_allows(wrapper, bed::make_partner(f.terran)) &&
                   !bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
}

BED_SCENARIO(search_side_ends_with_the_search) {
    bed::Fixture f = bed::start(folder);
    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    void* wrapper = bed::filter_slot(f.record);

    bool allowed = false;
    if (!bed::run_find_offer(bed::make_script_action(bed::kFindBuyOffer),
                             [&] { allowed = bed::wrapper_allows(wrapper, bed::make_partner(f.argon)); })) {
        bed::check("the find-offer handler carries the mod's detour", false);
        return;
    }
    bed::check("a sale inside find_buy_offer allows argon", allowed);
    bed::check("after the search returns, a bare check is unclassified again",
               !bed::wrapper_allows(wrapper, bed::make_partner(f.argon)));
}
