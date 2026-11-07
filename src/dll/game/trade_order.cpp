#include "game/trade_order.h"

#include <x4native.h>

namespace action_based_trade_restriction::game {
namespace {

// An order's parameter list stores 16-byte script values in order; each value's payload sits at +0x08.
constexpr std::size_t kOffsetScriptValuePayload = 0x08;

constexpr int kHandleMaskTrade = 2;
constexpr int kHandleMaskComponent = 4;

Component component_by_id(std::uint64_t id) {
    auto* game = x4n::game();
    const auto* offsets = x4n::detail::offsets();
    if (!id || !game || !offsets) return Component();
    if (void* reg = dereference_slot(offsets->component_registry); reg && game->ComponentRegistry_Find) {
        if (void* found = game->ComponentRegistry_Find(reg, id, kHandleMaskComponent)) {
            return Component(found);
        }
    }
    void* map = dereference_slot(offsets->order_id_map);
    return Component(
        map && game->AutoIDMap_Lookup
            ? game->AutoIDMap_Lookup(map, id, kHandleMaskComponent)
            : nullptr
    );
}

}  // namespace

Component Trade::seller() const {
    return component_by_id(field<std::uint64_t>(x4n::detail::offsets()->trade_seller));
}

Component Trade::buyer() const {
    return component_by_id(field<std::uint64_t>(x4n::detail::offsets()->trade_buyer));
}

std::optional<Trade> Order::trade() const {
    auto* game = x4n::game();
    const auto* offsets = x4n::detail::offsets();
    if (
        !game ||
        !offsets ||
        !game->AutoIDMap_Lookup ||
        !field<void*>(offsets->order_definition)
    ) {
        return std::nullopt;
    }
    const auto handle = field<std::uint64_t>(offsets->order_param_values + kOffsetScriptValuePayload);
    void* map = dereference_slot(offsets->order_id_map);
    void* trade = handle && map ? game->AutoIDMap_Lookup(map, handle, kHandleMaskTrade) : nullptr;
    if (!trade) return std::nullopt;
    return Trade(trade);
}

std::optional<Order> Controllable::current_order() const {
    if (!*this) return std::nullopt;
    void* order = x4n::ship::Ship(static_cast<const X4Component*>(object())->id).current_order();
    if (!order) return std::nullopt;
    return Order(order);
}

std::uint32_t ScriptAction::command() const {
    const auto* offsets = x4n::detail::offsets();
    if (!*this || !offsets) return 0;
    return field<std::uint32_t>(offsets->script_action_command_id);
}

}  // namespace action_based_trade_restriction::game
