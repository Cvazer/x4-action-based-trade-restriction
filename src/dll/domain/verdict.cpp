#include "domain/verdict.h"

#include <algorithm>

namespace action_based_trade_restriction {

bool ActionRule::passes(std::uint64_t owner) const {
    if (!enabled || owners.empty()) return true;

    const bool listed = std::ranges::find(owners, owner) != owners.end();
    return whitelist ? listed : !listed;
}

Decision decide(
    const Rule& rule,
    Operation operation,
    bool own_allows,
    std::uint64_t partner_owner
) {
    if (!own_allows) return Decision::Deny;

    switch (operation) {
        case Operation::Sell:
            return rule.sell.passes(partner_owner) ? Decision::Allow : Decision::Deny;
        case Operation::Buy:
            return rule.buy.passes(partner_owner) ? Decision::Allow : Decision::Deny;
        case Operation::Unknown:
            // Defensive default when called directly with an unclassified operation; the wrapper
            // evaluates unclassified checks against both sets.
            return Decision::Allow;
    }
    return Decision::Allow;
}

}  // namespace action_based_trade_restriction
