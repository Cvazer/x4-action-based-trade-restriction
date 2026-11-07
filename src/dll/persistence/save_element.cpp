#include "persistence/save_element.h"

#include <charconv>
#include <string>
#include <string_view>

namespace action_based_trade_restriction::save {
namespace {

std::string join_factions(const std::vector<std::string>& factions) {
    std::string text;
    for (const std::string& faction : factions) {
        if (!text.empty()) text.push_back(' ');
        text += faction;
    }
    return text;
}

std::vector<std::string> split_factions(std::string_view text) {
    std::vector<std::string> factions;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find(' ', start);
        const std::string_view part = text.substr(
            start,
            end == std::string_view::npos ? end : end - start
        );
        if (!part.empty()) factions.emplace_back(part);
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return factions;
}

std::optional<game::XmlNode> find_child(game::XmlNode parent, std::int32_t element_id) {
    for (game::XmlNode child : parent.children()) {
        if (child.element() == element_id) return child;
    }
    return std::nullopt;
}

bool write_side(
    game::XmlNode parent,
    std::int32_t side_element,
    const ActionRule& action,
    const Names& names
) {
    const auto node = game::XmlNode::create(side_element);
    if (!node) return false;
    node->set_attribute(names.enabled.number(), action.enabled ? "1" : "0");
    node->set_attribute(names.whitelist.number(), action.whitelist ? "1" : "0");
    node->set_attribute(names.factions.number(), join_factions(action.faction_ids));
    parent.append(*node);
    return true;
}

std::expected<ActionRule, const char*> read_side(
    game::XmlNode parent,
    std::int32_t side_element,
    const Names& names
) {
    const auto node = find_child(parent, side_element);
    if (!node) return std::unexpected("the element misses a side");

    const auto enabled = node->attribute(names.enabled.number());
    const auto whitelist = node->attribute(names.whitelist.number());
    const auto factions = node->attribute(names.factions.number());
    if (!enabled || !whitelist || !factions) {
        return std::unexpected("a side misses an attribute");
    }

    ActionRule action;
    action.enabled = *enabled == "1";
    action.whitelist = *whitelist == "1";
    action.faction_ids = split_factions(*factions);
    return action;
}

}  // namespace

std::optional<Names> register_names() {
    const auto blacklist = x4n::savexml::register_tag("blacklist");
    const auto id = x4n::savexml::register_tag("id");
    const auto root = x4n::savexml::register_tag("action_based_trade_restriction");
    const auto sell = x4n::savexml::register_tag("sell");
    const auto buy = x4n::savexml::register_tag("buy");
    const auto enabled = x4n::savexml::register_tag("enabled");
    const auto whitelist = x4n::savexml::register_tag("whitelist");
    const auto factions = x4n::savexml::register_tag("factions");
    if (!blacklist || !id || !root || !sell || !buy || !enabled || !whitelist || !factions) {
        return std::nullopt;
    }
    return Names{
        .blacklist = *blacklist,
        .id = *id,
        .root = *root,
        .sell = *sell,
        .buy = *buy,
        .enabled = *enabled,
        .whitelist = *whitelist,
        .factions = *factions,
    };
}

std::int32_t list_id(game::XmlNode list_node, const Names& names) {
    const auto text = list_node.attribute(names.id.number());
    if (!text || text->empty()) return 0;

    std::int32_t value = 0;
    const char* begin = text->data();
    const char* end = begin + text->size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end || value <= 0) return 0;
    return value;
}

bool write(
    game::XmlNode list_node,
    const Rule& rule,
    const Names& names
) {
    const auto root = game::XmlNode::create(names.root.number());
    if (!root) return false;
    if (!write_side(*root, names.sell.number(), rule.sell, names)) return false;
    if (!write_side(*root, names.buy.number(), rule.buy, names)) return false;
    list_node.append(*root);
    return true;
}

std::expected<std::optional<Rule>, const char*> read(game::XmlNode list_node, const Names& names) {
    const auto root = find_child(list_node, names.root.number());
    if (!root) return std::optional<Rule>{};

    Rule rule;
    rule.id = list_id(list_node, names);
    if (rule.id <= 0) return std::unexpected("the list carries no usable id");

    auto sell = read_side(*root, names.sell.number(), names);
    if (!sell) return std::unexpected(sell.error());
    auto buy = read_side(*root, names.buy.number(), names);
    if (!buy) return std::unexpected(buy.error());

    rule.sell = *sell;
    rule.buy = *buy;
    return std::optional<Rule>{rule};
}

}  // namespace action_based_trade_restriction::save
