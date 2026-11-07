// Engine blacklist filter, record and registry handles.

#pragma once

#include "game/component.h"
#include "game/handle.h"

#include <cstdint>
#include <optional>
#include <string>

namespace action_based_trade_restriction::game {

// A compiled blacklist filter stored in a list record's filter slot (+0x70): either the engine's own
// filter or the mod's wrapper filter.
class Filter : public Handle {
public:
    using Handle::Handle;

    // Evaluates whether the filter permits `partner` (vtable slot 3).
    bool allows(Component partner) const;

    // Destroys the filter through its virtual destructor (vtable slot 0, deleting flag 1).
    void destroy() const;
};

// A blacklist record in the game's blacklist registry.
class ListRecord : public Handle {
public:
    using Handle::Handle;

    std::string name() const;
    std::int32_t type() const;
    Filter filter() const;
    void set_filter(Filter filter) const;
};

// The engine's central registry of blacklists.
namespace lists {

bool available();
std::optional<ListRecord> record(std::int32_t id);

// Resolves the runtime type index for trade blacklists (`objectactivity`), or nullopt if unavailable.
std::optional<std::int32_t> trade_type();

}  // namespace lists

}  // namespace action_based_trade_restriction::game
