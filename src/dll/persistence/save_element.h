// XML persistence for a trade list's sell and buy rules inside its `<blacklist>` save element:
//
//   <action_based_trade_restriction>
//     <sell enabled="1" whitelist="0" factions="terran"/>
//     <buy enabled="1" whitelist="1" factions="argon paranid"/>
//   </action_based_trade_restriction>
//
// Both sides are always written when a rule is active. Element and attribute names are registered with
// X4Native's SaveXML table at extension load so the engine's parser and formatter recognize them.

#pragma once

#include "domain/rules.h"
#include "game/xml_node.h"

#include <x4native.h>

#include <cstdint>
#include <expected>
#include <optional>

namespace action_based_trade_restriction::save {

// Session SaveXML tags for every element and attribute name the mod reads or writes.
struct Names {
    x4n::savexml::SaveXmlTag blacklist;  // <blacklist> element (owned by the game)
    x4n::savexml::SaveXmlTag id;         // id="..." attribute on <blacklist> (owned by the game)
    x4n::savexml::SaveXmlTag root;       // <action_based_trade_restriction> child element
    x4n::savexml::SaveXmlTag sell;
    x4n::savexml::SaveXmlTag buy;
    x4n::savexml::SaveXmlTag enabled;
    x4n::savexml::SaveXmlTag whitelist;
    x4n::savexml::SaveXmlTag factions;
};

// Registers the SaveXML names for the current session, or returns nullopt if unavailable.
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
// - returns `nullopt` if `list_node` carries no `<action_based_trade_restriction>` child;
// - returns an error string if the child element is malformed;
// - returns the parsed `Rule` otherwise.
std::expected<std::optional<Rule>, const char*> read(game::XmlNode list_node, const Names& names);

}  // namespace action_based_trade_restriction::save
