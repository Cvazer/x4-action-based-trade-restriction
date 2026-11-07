// What a fetch hands a compiled match chain, and which side the ship's order implies.
//
// The engine asks the mod for a list's filter before it searches for a partner. For a trade list it
// must receive the wrapper; for a travel or activity lookup, the engine's own filter. When the fetch
// has a ship, the ship's current order decides whether the check is a sale or a purchase.

#include "common.h"
#include "scenario.h"

BED_SCENARIO(fetch_by_order) {
    bed::Fixture f = bed::start(folder);

    void* second_engine = bed::make_engine_filter();
    void* second_record = bed::make_record(1, "Scoped Buy", second_engine);

    bed::raise("on_game_loaded");
    bed::send("1|sell:10:terran|buy:10:argon");
    bed::send("2|sell:10:argon|buy:10:terran");

    void* wrapper = bed::filter_slot(f.record);
    void* second_wrapper = bed::filter_slot(second_record);
    bed::check("a second stored entry wraps its own list", second_wrapper != second_engine);

    auto* fetch = reinterpret_cast<void* (*)(void*, int, int, int)>(
        bed::detour_for("BlacklistFetchForControllable"));
    if (!fetch) {
        bed::check("the fetch carries the mod's detour", false);
        return;
    }

    const std::uint64_t deal = 0x1234;
    void* seller_ship = bed::make_controllable(bed::make_order(deal));
    void* buyer_ship = bed::make_controllable(bed::make_order(deal));
    void* other_ship = bed::make_controllable(bed::make_order(deal));
    void* stranger_ship = bed::make_controllable(bed::make_order(0x9999));
    bed::make_trade(seller_ship, buyer_ship, deal);

    bed::engine().fetched_filter = wrapper;
    bed::check("the fetch hands back the wrapped filter for a list of the trade type",
               fetch(seller_ship, 1, 0, 0) == wrapper);
    bed::engine().fetched_filter = f.engine_filter;
    bed::check("the fetch leaves a travel or sectoractivity lookup untouched",
               fetch(seller_ship, 0, 0, 0) == f.engine_filter);
    bed::engine().fetched_filter = wrapper;

    void* terran = bed::make_partner(f.terran);
    void* argon = bed::make_partner(f.argon);
    bed::check("a fetched filter judges a check by the ship's order",
               [&] {
                   fetch(seller_ship, 1, 0, 0);
                   const bool sells = !bed::wrapper_allows(wrapper, terran);
                   fetch(buyer_ship, 1, 0, 0);
                   const bool buys = !bed::wrapper_allows(wrapper, argon);
                   return sells && buys;
               }());

    bed::check("one list's fetch does not judge another list's check",
               [&] {
                   fetch(seller_ship, 1, 0, 0);
                   void* neutral = bed::make_partner(bed::add_faction("neutral"));
                   return bed::wrapper_allows(second_wrapper, neutral) &&
                          bed::wrapper_allows(wrapper, neutral) &&
                          !bed::wrapper_allows(second_wrapper, argon);
               }());

    bed::check("a ship with no trade order is judged by every enabled set",
               [&] {
                   fetch(other_ship, 1, 0, 0);
                   const bool sell_side = !bed::wrapper_allows(wrapper, terran);
                   fetch(stranger_ship, 1, 0, 0);
                   const bool buy_side = !bed::wrapper_allows(wrapper, argon);
                   return sell_side && buy_side;
               }());
}
