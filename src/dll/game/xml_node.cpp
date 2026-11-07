#include "game/xml_node.h"

#include <x4native.h>

namespace action_based_trade_restriction::game {
namespace {

// A small vector inside an XmlNode: its entries inline at its start while its capacity is the inline
// capacity, otherwise a pointer to them there. Every offset is from the node.
class List : public Handle {
public:
    struct Layout {
        std::size_t data_offset;
        std::size_t capacity_offset;
        std::size_t count_offset;
        std::size_t entry_size;
    };

    List(XmlNode node, Layout layout) : Handle(node.object()), layout_(layout) {}

    void* data() const { return *this ? static_cast<std::uint8_t*>(object()) + layout_.data_offset : nullptr; }
    std::size_t count() const { return *this ? field<std::size_t>(layout_.count_offset) : 0; }

protected:
    void* entry(std::size_t index) const {
        auto* base = (field<std::size_t>(layout_.capacity_offset) == X4_XML_NODE_INLINE_CAPACITY)
            ? static_cast<std::uint8_t*>(data())
            : field<std::uint8_t*>(layout_.data_offset);
        return base + index * layout_.entry_size;
    }

private:
    Layout layout_;
};

class Attribute : public Handle {
public:
    using Handle::Handle;

    std::int32_t id() const { return field<std::int32_t>(x4n::detail::offsets()->xml_attribute_id); }
    const std::string& value() const { return field<std::string>(x4n::detail::offsets()->xml_attribute_value); }
};

class AttributeList : public List {
public:
    static Layout layout() {
        const auto* offsets = x4n::detail::offsets();
        return {
            offsets->xml_node_attributes,
            offsets->xml_node_attribute_capacity,
            offsets->xml_node_attribute_count,
            offsets->xml_attribute_size,
        };
    }

    explicit AttributeList(XmlNode node) : List(node, layout()) {}

    std::optional<std::string> find_value(std::int32_t id) const {
        for (std::size_t i = 0; i < count(); ++i) {
            const Attribute attr(entry(i));
            if (attr.id() == id) return attr.value();
        }
        return std::nullopt;
    }
};

class ChildList : public List {
public:
    static Layout layout() {
        const auto* offsets = x4n::detail::offsets();
        return {
            offsets->xml_node_children,
            offsets->xml_node_child_capacity,
            offsets->xml_node_child_count,
            sizeof(void*),
        };
    }

    explicit ChildList(XmlNode node) : List(node, layout()) {}

    std::vector<XmlNode> all() const {
        std::vector<XmlNode> nodes;
        nodes.reserve(count());
        for (std::size_t i = 0; i < count(); ++i) {
            nodes.emplace_back(*static_cast<void* const*>(entry(i)));
        }
        return nodes;
    }
};

}  // namespace

std::optional<XmlNode> XmlNode::create(std::int32_t element) {
    auto* game = x4n::game();
    const auto* offsets = x4n::detail::offsets();
    if (
        !game ||
        !offsets ||
        !game->GameAlloc ||
        !game->XmlNode_SetAttribute ||
        !game->XmlNodeVector_PushBack
    ) {
        return std::nullopt;
    }
    void* memory = game->GameAlloc(offsets->xml_node_size, 0, 0, 0, X4_XML_NODE_ALIGNMENT);
    if (!memory) return std::nullopt;

    XmlNode node(memory);
    node.field<std::uint16_t>(offsets->xml_node_source_line) = X4_XML_NODE_NO_SOURCE_LINE;
    node.field<std::int32_t>(offsets->xml_node_source_column) = X4_XML_NODE_NO_SOURCE_COLUMN;
    node.field<std::uint64_t>(offsets->xml_node_prefix) = 0;
    node.field<std::uint64_t>(offsets->xml_node_text) = 0;
    for (const List::Layout& list : {AttributeList::layout(), ChildList::layout()}) {
        node.field<std::size_t>(list.capacity_offset) = X4_XML_NODE_INLINE_CAPACITY;
        node.field<std::size_t>(list.count_offset) = 0;
    }
    node.field<std::uint8_t>(offsets->xml_node_flags) = X4_XML_NODE_FLAG_ELEMENT;
    node.field<std::int32_t>(offsets->xml_node_element) = element;
    return node;
}

std::int32_t XmlNode::element() const {
    const auto* offsets = x4n::detail::offsets();
    if (!*this || (field<std::uint8_t>(offsets->xml_node_flags) & X4_XML_NODE_FLAG_TEXT)) return 0;
    return field<std::int32_t>(offsets->xml_node_element);
}

std::vector<XmlNode> XmlNode::children() const { return ChildList(*this).all(); }

std::optional<std::string> XmlNode::attribute(std::int32_t attribute_id) const {
    return AttributeList(*this).find_value(attribute_id);
}

void XmlNode::set_attribute(std::int32_t attribute_id, const std::string& value) const {
    if (*this) x4n::game()->XmlNode_SetAttribute(object(), attribute_id, value.c_str());
}

void XmlNode::append(XmlNode child) const {
    if (!*this || !child) return;
    void* const item = child.object();
    x4n::game()->XmlNodeVector_PushBack(ChildList(*this).data(), &item);
}

}  // namespace action_based_trade_restriction::game
