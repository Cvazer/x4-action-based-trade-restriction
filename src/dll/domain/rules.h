// Domain model for a trade list's sell and buy rules, and JSON serialization for UI messages.
//
// Each entry exchanged between the UI and the DLL has the shape:
//
//   {"list":3,"sell":{"enabled":true,"whitelist":false,"factions":["terran"]},
//    "buy":{"enabled":true,"whitelist":true,"factions":["argon","paranid"]}}

#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace action_based_trade_restriction {

using OwnerResolver = std::function<std::uint64_t(const std::string&)>;

// Configuration and resolved owner IDs for one action (sell or buy) of a trade list.
struct ActionRule {
    bool enabled = false;
    bool whitelist = false;  // true: allow only listed owners; false: refuse listed owners
    std::vector<std::string> faction_ids;
    std::vector<std::uint64_t> owners;

    // Returns true if this action permits a partner with the given owner ID.
    // An inactive or empty action permits everyone.
    bool passes(std::uint64_t owner) const;

    // Resolves `faction_ids` into engine owner values. If any faction fails to resolve, switches this
    // action off so it restricts nobody rather than everybody, and returns the unresolved faction ID.
    std::optional<std::string> resolve_owners(const OwnerResolver& owner_of);
};

// Sell and buy rules associated with a single trade blacklist.
struct Rule {
    std::int32_t id = 0;
    std::string name;
    ActionRule sell;
    ActionRule buy;

    // A rule is active when at least one of its actions is enabled.
    bool is_active() const { return sell.enabled || buy.enabled; }

    // Resolves both actions' factions and returns any faction IDs that failed to resolve.
    std::vector<std::string> resolve_owners(const OwnerResolver& owner_of);
};

std::expected<Rule, const char*> parse_entry(std::string_view text);
std::string encode_entry(const Rule& rule);

}  // namespace action_based_trade_restriction
