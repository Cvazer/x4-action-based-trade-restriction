#include "runtime/operation.h"

#include <cstdint>
#include <vector>

#include <x4native.h>

namespace action_based_trade_restriction::operation {
namespace {

Operation operation_from_action(game::ScriptAction action) {
    const std::uint32_t command = action.command();
    if (command == X4_SCRIPT_COMMAND_FIND_SELL_OFFER) return Operation::Buy;
    if (command == X4_SCRIPT_COMMAND_FIND_BUY_OFFER) return Operation::Sell;
    return Operation::Unknown;
}

Operation operation_from_order(game::Controllable ship) {
    const auto order = ship.current_order();
    const auto trade = order ? order->trade() : std::nullopt;
    if (!trade) return Operation::Unknown;
    if (trade->seller() == ship.component()) return Operation::Sell;
    if (trade->buyer() == ship.component()) return Operation::Buy;
    return Operation::Unknown;
}

class ThreadContext {
public:
    void enter_search(game::ScriptAction action) {
        searches_.push_back(operation_from_action(action));
    }

    void leave_search() {
        if (!searches_.empty()) searches_.pop_back();
    }

    void record_fetch(game::Controllable ship, game::Filter filter) {
        fetched_ship_ = ship;
        fetched_filter_ = filter;
    }

    Operation consume_operation_for(game::Filter filter) {
        Operation current = searches_.empty() ? Operation::Unknown : searches_.back();
        if (fetched_filter_ == filter && fetched_ship_) {
            const game::Controllable ship = fetched_ship_;
            fetched_ship_ = game::Controllable();
            fetched_filter_ = game::Filter();
            if (current == Operation::Unknown) {
                current = operation_from_order(ship);
            }
        }
        return current;
    }

    void reset() { *this = ThreadContext{}; }

private:
    std::vector<Operation> searches_;
    game::Controllable fetched_ship_;
    game::Filter fetched_filter_;
};

thread_local ThreadContext t_context;

}  // namespace

void enter_search(game::ScriptAction action) { t_context.enter_search(action); }

void leave_search() { t_context.leave_search(); }

void record_fetch(game::Controllable ship, game::Filter filter) {
    t_context.record_fetch(ship, filter);
}

Operation consume_operation_for(game::Filter filter) {
    return t_context.consume_operation_for(filter);
}

void reset() { t_context.reset(); }

}  // namespace action_based_trade_restriction::operation
