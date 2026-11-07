#include "domain/rule_table.h"

#include <algorithm>

namespace action_based_trade_restriction {

void RuleTable::store(const Rule& rule) {
    const auto found = std::ranges::find_if(
        rules_,
        [id = rule.id](const Rule& existing) { return existing.id == id; }
    );
    if (found != rules_.end()) {
        *found = rule;
        return;
    }
    rules_.push_back(rule);
}

const Rule* RuleTable::find(std::int32_t id) const {
    const auto found = std::ranges::find_if(
        rules_,
        [id](const Rule& rule) { return rule.id == id; }
    );
    return found != rules_.end() ? &*found : nullptr;
}

void RuleTable::erase(std::int32_t id) {
    std::erase_if(
        rules_,
        [id](const Rule& rule) { return rule.id == id; }
    );
}

void RuleTable::prune(const std::function<bool(std::int32_t)>& list_exists) {
    std::erase_if(
        rules_,
        [&](const Rule& rule) { return !list_exists(rule.id); }
    );
}

std::string RuleTable::encode() const {
    std::string table = "[";
    for (const Rule& rule : rules_) {
        if (table.size() > 1) table.push_back(',');
        table += encode_entry(rule);
    }
    table.push_back(']');
    return table;
}

}  // namespace action_based_trade_restriction
