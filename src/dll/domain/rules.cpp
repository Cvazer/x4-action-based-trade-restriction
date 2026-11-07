#include "domain/rules.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>

namespace action_based_trade_restriction {
namespace {

using nlohmann::json;
using nlohmann::ordered_json;

std::expected<ActionRule, const char*> parse_action(const json& value) {
    if (!value.is_object()) return std::unexpected("a set is an object");
    const auto enabled = value.find("enabled");
    const auto whitelist = value.find("whitelist");
    const auto factions = value.find("factions");
    if (enabled == value.end() || whitelist == value.end() || factions == value.end()) {
        return std::unexpected("a set needs enabled, whitelist and factions");
    }
    if (!enabled->is_boolean() || !whitelist->is_boolean()) {
        return std::unexpected("the enabled and whitelist flags are booleans");
    }
    if (!factions->is_array()) return std::unexpected("a set's factions are a list");

    ActionRule action;
    action.enabled = enabled->get<bool>();
    action.whitelist = whitelist->get<bool>();
    for (const json& faction : *factions) {
        if (!faction.is_string()) return std::unexpected("a faction is a faction id");
        action.faction_ids.push_back(faction.get<std::string>());
    }
    return action;
}

ordered_json encode_action(const ActionRule& action) {
    return ordered_json{
        {"enabled", action.enabled},
        {"whitelist", action.whitelist},
        {"factions", action.faction_ids},
    };
}

}  // namespace

std::optional<std::string> ActionRule::resolve_owners(const OwnerResolver& owner_of) {
    owners.clear();
    if (!enabled) return std::nullopt;

    owners.reserve(faction_ids.size());
    for (const std::string& faction : faction_ids) {
        const std::uint64_t owner = owner_of(faction);
        if (!owner) {
            enabled = false;
            owners.clear();
            return faction;
        }
        owners.push_back(owner);
    }
    return std::nullopt;
}

std::vector<std::string> Rule::resolve_owners(const OwnerResolver& owner_of) {
    std::vector<std::string> unresolved;
    if (auto failed = sell.resolve_owners(owner_of)) unresolved.push_back(*failed);
    if (auto failed = buy.resolve_owners(owner_of)) unresolved.push_back(*failed);
    return unresolved;
}

std::expected<Rule, const char*> parse_entry(std::string_view text) {
    const json entry = json::parse(text, nullptr, false);
    if (entry.is_discarded() || !entry.is_object()) return std::unexpected("an entry is a JSON object");

    Rule rule;
    const auto list = entry.find("list");
    if (list == entry.end() || !list->is_number_integer()) return std::unexpected("an entry needs its list id");
    const auto id = list->get<std::int64_t>();
    if (id <= 0 || id > std::numeric_limits<std::int32_t>::max()) {
        return std::unexpected("a list id must be a positive number");
    }
    rule.id = static_cast<std::int32_t>(id);

    bool has_action = false;
    for (const auto& [key, value] : entry.items()) {
        if (key == "list") continue;
        auto action = parse_action(value);
        if (!action) return std::unexpected(action.error());
        if (key == "sell") rule.sell = *action;
        else if (key == "buy") rule.buy = *action;
        else return std::unexpected("a set's name must be sell or buy");
        has_action = true;
    }
    if (!has_action) return std::unexpected("an entry needs a list and at least one set");
    return rule;
}

std::string encode_entry(const Rule& rule) {
    return ordered_json{
        {"list", rule.id},
        {"sell", encode_action(rule.sell)},
        {"buy", encode_action(rule.buy)},
    }.dump();
}

}  // namespace action_based_trade_restriction
