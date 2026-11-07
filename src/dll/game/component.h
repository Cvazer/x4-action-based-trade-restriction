// Engine component handle and faction ownership resolution.

#pragma once

#include "game/handle.h"

#include <cstdint>
#include <string>

namespace action_based_trade_restriction::game {

// A game entity that can participate in a trade (ship or station).
class Component : public Handle {
public:
    using Handle::Handle;

    // The component's faction owner identity (`FactionClass*` address as a 64-bit integer),
    // which is the value the engine's own owner filter compares against.
    std::uint64_t owner() const;
};

// Resolves a faction ID string to the engine's faction owner value (`FactionClass*` cast to uint64),
// or 0 if the faction is unknown.
std::uint64_t faction_owner(const std::string& faction_id);

}  // namespace action_based_trade_restriction::game
