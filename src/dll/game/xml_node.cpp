#include "game/xml_node.h"

#include <x4native.h>

namespace action_based_trade_restriction::game {

std::optional<XmlNode> XmlNode::create(std::int32_t element) {
    void* node = x4n::savexml::create_node(element);
    if (!node) return std::nullopt;
    return XmlNode(node);
}

std::int32_t XmlNode::element() const { return x4n::savexml::element(object()); }

std::vector<XmlNode> XmlNode::children() const {
    std::vector<XmlNode> nodes;
    for (void* child : x4n::savexml::children(object())) nodes.emplace_back(child);
    return nodes;
}

std::optional<std::string> XmlNode::attribute(std::int32_t attribute_id) const {
    const char* value = x4n::savexml::get_attribute(object(), attribute_id);
    if (!value) return std::nullopt;
    return std::string(value);
}

void XmlNode::set_attribute(std::int32_t attribute_id, const std::string& value) const {
    x4n::savexml::set_attribute(object(), attribute_id, value.c_str());
}

void XmlNode::append(XmlNode child) const { x4n::savexml::append_child(object(), child.object()); }

}  // namespace action_based_trade_restriction::game
