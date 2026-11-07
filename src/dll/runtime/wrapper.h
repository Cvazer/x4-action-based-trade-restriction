// Proxy filter installed in a trade blacklist's filter slot (+0x70) over the engine's compiled filter.
//
// The wrapper matches the engine's binary filter header and 4-slot vtable layout. When the engine tests
// a candidate partner against the list, the wrapper first asks the underlying engine filter and then
// narrows the verdict using the list's sell and buy restrictions.

#pragma once

#include "domain/rule_table.h"
#include "game/blacklist.h"

#include <cstdint>

namespace action_based_trade_restriction::wrapper {

// Binds the rule table that active wrappers consult during partner checks.
void bind(const RuleTable& rules);

// Ensures `record` is wrapped for `list_id` if it is a trade list (`record.type() == trade_type`).
// Returns true if the record now holds a wrapper.
bool wrap(
    game::ListRecord record,
    std::int32_t list_id,
    std::int32_t trade_type
);

// Restores `record`'s original engine filter and deletes its wrapper if `record` holds its own wrapper.
void unwrap(game::ListRecord record);

// Returns the list ID of `filter` if it is one of the mod's wrappers, or 0 otherwise.
std::int32_t list_id_of(game::Filter filter);

// Returns the wrapper covering `filter` if one exists, or `filter` itself otherwise.
game::Filter wrapper_for(game::Filter filter);

// Restores all wrapped list records to their original engine filters and deletes all wrappers.
void unwrap_all();

}  // namespace action_based_trade_restriction::wrapper
