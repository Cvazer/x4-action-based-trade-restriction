// In-memory repository of active trade-list rules, keyed by blacklist ID.

#pragma once

#include "domain/rules.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace action_based_trade_restriction {

class RuleTable {
public:
    // Inserts or replaces the rule for `rule.id`.
    void store(const Rule& rule);

    // Returns the stored rule for `id`, or nullptr if none exists.
    const Rule* find(std::int32_t id) const;

    void erase(std::int32_t id);

    // Removes every rule whose list ID no longer satisfies `list_exists`.
    void prune(const std::function<bool(std::int32_t)>& list_exists);

    // Serializes all stored rules as a JSON array.
    std::string encode() const;

    const std::vector<Rule>& all() const { return rules_; }

    void clear() { rules_.clear(); }
    std::size_t size() const { return rules_.size(); }

private:
    std::vector<Rule> rules_;
};

}  // namespace action_based_trade_restriction
