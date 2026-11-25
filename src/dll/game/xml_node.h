// Handle over a node in the engine's in-memory SaveXML DOM tree.

#pragma once

#include "game/handle.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace action_based_trade_restriction::game {

class XmlNode : public Handle {
public:
    using Handle::Handle;

    // A new element node, built by X4Native as the game builds one.
    static std::optional<XmlNode> create(std::int32_t element);

    // The node's SaveXML element number, or 0 for text/null nodes.
    std::int32_t element() const;
    std::vector<XmlNode> children() const;
    std::optional<std::string> attribute(std::int32_t attribute_id) const;

    void set_attribute(std::int32_t attribute_id, const std::string& value) const;
    void append(XmlNode child) const;
};

}  // namespace action_based_trade_restriction::game
