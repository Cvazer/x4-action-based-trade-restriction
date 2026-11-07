// Thread-local trade-operation tracking for blacklist filter checks.
//
// A filter's `allows()` check receives only the candidate partner, so the active trade operation is
// tracked per thread:
// 1. Inside a `find_sell_offer` or `find_buy_offer` script action, the action's command ID determines
//    the operation for the duration of the search (supporting nested searches).
// 2. Otherwise, when a filter was just fetched for a controllable ship, that ship's current trade order
//    determines the operation for that single filter check.
// 3. If neither source establishes the operation, it is `Operation::Unknown`.

#pragma once

#include "domain/verdict.h"
#include "game/blacklist.h"
#include "game/trade_order.h"

namespace action_based_trade_restriction::operation {

// Pushes the operation implied by `action` onto the current thread's search stack.
void enter_search(game::ScriptAction action);

// Pops the innermost search operation from the current thread's search stack.
void leave_search();

// Records that a blacklist fetch returned `filter` for `ship`; consumed by the next check on `filter`.
void record_fetch(game::Controllable ship, game::Filter filter);

// Resolves the operation for a check on `filter`, consuming any pending fetch recorded for it.
Operation consume_operation_for(game::Filter filter);

// Clears all thread-local state.
void reset();

}  // namespace action_based_trade_restriction::operation
