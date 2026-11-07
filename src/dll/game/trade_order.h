// Engine handles for trades, ship orders, controllables and script actions.

#pragma once

#include "game/component.h"
#include "game/handle.h"

#include <cstdint>
#include <optional>

namespace action_based_trade_restriction::game {

// A trade deal between a selling and a buying component.
class Trade : public Handle {
public:
    using Handle::Handle;

    Component seller() const;
    Component buyer() const;
};

// An AI order held by a controllable entity.
class Order : public Handle {
public:
    using Handle::Handle;

    // The trade carried in the order's first script parameter, or nullopt if it is not a trade order.
    std::optional<Trade> trade() const;
};

// A ship or station capable of executing orders.
class Controllable : public Handle {
public:
    using Handle::Handle;

    Component component() const { return Component(object()); }

    // The order it executes, as `x4n::ship::Ship::current_order` picks it. None for a controllable
    // that is not a ship.
    std::optional<Order> current_order() const;
};

// A running MD/AI script action (such as `find_sell_offer` or `find_buy_offer`).
class ScriptAction : public Handle {
public:
    using Handle::Handle;

    std::uint32_t command() const;
};

}  // namespace action_based_trade_restriction::game
