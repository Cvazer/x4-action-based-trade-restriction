// XML persistence for a trade list's sell and buy rules inside its `<blacklist>` save element:
//
//   <blacklist id="2" ...>
//     <action_based_trade_restriction.rules>
//       <action_based_trade_restriction.sell action_based_trade_restriction.enabled="1"
//           action_based_trade_restriction.whitelist="0" action_based_trade_restriction.factions="terran"/>
//       <action_based_trade_restriction.buy action_based_trade_restriction.enabled="1"
//           action_based_trade_restriction.whitelist="1" action_based_trade_restriction.factions="argon paranid"/>
//     </action_based_trade_restriction.rules>
//   </blacklist>
//
// The mod's names are registered with X4Native's SaveXML table at extension load, under the mod's own
// id, so the engine's parser and formatter recognize them. `blacklist` and `id` are the game's names and
// are read through game_number().

#pragma once

#include "domain/rules.h"
#include "game/xml_node.h"

#include <x4native.h>

#include <cstdint>
#include <expected>
#include <optional>

namespace action_based_trade_restriction::save {

// Session SaveXML names for every element and attribute the mod reads or writes.
struct Names {
    std::int32_t blacklist;               // <blacklist> element number (owned by the game)
    std::int32_t id;                      // id="..." attribute number on <blacklist> (owned by the game)
    x4n::savexml::SaveXmlTag root;        // <action_based_trade_restriction.rules> child element
    x4n::savexml::SaveXmlTag sell;
    x4n::savexml::SaveXmlTag buy;
    x4n::savexml::SaveXmlTag enabled;
    x4n::savexml::SaveXmlTag whitelist;
    x4n::savexml::SaveXmlTag factions;
};

// Registers the mod's SaveXML names for the current session, or returns nullopt if unavailable.
std::optional<Names> register_names();

// Reads the positive integer `id` attribute of a `<blacklist>` node, or 0 if missing/invalid.
std::int32_t list_id(game::XmlNode list_node, const Names& names);

// Appends the mod's child element for `rule` to `list_node`. Returns false if node creation fails.
bool write(
    game::XmlNode list_node,
    const Rule& rule,
    const Names& names
);

// Reads a list's rule from its `<blacklist>` node:
// - returns `nullopt` if `list_node` carries no `<action_based_trade_restriction.rules>` child;
// - returns an error string if the child element is malformed;
// - returns the parsed `Rule` otherwise.
std::expected<std::optional<Rule>, const char*> read(game::XmlNode list_node, const Names& names);

}  // namespace action_based_trade_restriction::save
