// Pure trade-restriction decision logic, independent of the game engine.
//
// Given the game's own filter verdict, the trade operation being performed, and a list's rule,
// `decide()` determines whether a prospective partner component is allowed.

#pragma once

#include "domain/rules.h"

#include <cstdint>

namespace action_based_trade_restriction {

// Which side of a trade a filter check belongs to.
enum class Operation { Unknown, Sell, Buy };

enum class Decision { Allow, Deny };

// Evaluates whether a trade partner is allowed.
// The rule only narrows the game's own filter verdict (`own_allows`), so if the game refuses the
// partner, the result is always `Decision::Deny`.
Decision decide(
    const Rule& rule,
    Operation operation,
    bool own_allows,
    std::uint64_t partner_owner
);

}  // namespace action_based_trade_restriction
