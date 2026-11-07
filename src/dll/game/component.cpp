#include "game/component.h"

#include <x4native.h>

namespace action_based_trade_restriction::game {

std::uint64_t Component::owner() const {
    const auto* offsets = x4n::detail::offsets();
    if (!*this || !offsets) return 0;
    return method<std::uint64_t (*)(void*)>(offsets->vtable_get_faction_id)(object());
}

std::uint64_t faction_owner(const std::string& faction_id) {
    return reinterpret_cast<std::uint64_t>(x4n::faction::find_class_by_id(faction_id.c_str()));
}

}  // namespace action_based_trade_restriction::game
