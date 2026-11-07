#include "runtime/wrapper.h"

#include "domain/verdict.h"
#include "runtime/operation.h"

#include <x4native.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace action_based_trade_restriction::wrapper {
namespace {

struct Wrapper;

// Four-slot virtual table matching the engine's compiled filter classes.
struct WrapperVtable {
    void (*destroy)(Wrapper*, int);
    void (*clear_children)(Wrapper*);
    Wrapper* (*clone)(Wrapper*, void*);
    bool (*test)(Wrapper*, void*);
};

struct Wrapper {
    // Binary-compatible engine filter header (+0x00 .. +0x3F).
    const WrapperVtable* vtable;        // +0x00
    void* child_slots[4];               // +0x08: inline child storage (unused)
    std::uint64_t capacity;             // +0x28: 0, so the engine sees no children
    std::uint64_t count;                // +0x30: 0
    std::uint8_t children_unowned;      // +0x38: 1, holders do not destroy child filters
    std::uint8_t shared;                // +0x39: 1, holders neither copy nor delete this filter
    std::uint8_t pad[6];                // +0x3A

    // Extension state.
    game::ListRecord record;
    std::int32_t list_id;
    game::Filter inner_filter;

    game::Filter as_filter() { return game::Filter(this); }

    void restore_slot() {
        if (record && record.filter() == as_filter()) {
            record.set_filter(inner_filter);
        }
        inner_filter = game::Filter();
    }
};

static_assert(offsetof(Wrapper, capacity) == 0x28, "engine reads child capacity at +0x28");
static_assert(offsetof(Wrapper, count) == 0x30, "engine reads child count at +0x30");
static_assert(offsetof(Wrapper, children_unowned) == 0x38, "engine reads children_unowned flag at +0x38");
static_assert(offsetof(Wrapper, shared) == 0x39, "engine reads shared flag at +0x39");

struct RefusalKey {
    std::int32_t list_id;
    Operation operation;
    std::uint64_t partner_owner;

    bool operator==(const RefusalKey&) const = default;
};

constexpr std::size_t kMaxLoggedRefusals = 256;

const RuleTable* g_rules = nullptr;
std::vector<Wrapper*> g_wrappers;
std::vector<RefusalKey> g_logged_refusals;
bool g_reported_nested_wrapper = false;

Wrapper* find_wrapper(game::Filter filter) {
    const auto found = std::ranges::find(g_wrappers, filter.object());
    return found != g_wrappers.end() ? *found : nullptr;
}

const char* describe_operation(Operation op) {
    switch (op) {
        case Operation::Sell:
            return "a sale";
        case Operation::Buy:
            return "a purchase";
        case Operation::Unknown:
            return "an unclassified check (both sets applied)";
    }
    return "an unclassified check (both sets applied)";
}

void log_refusal(
    const Rule& rule,
    Operation op,
    std::uint64_t partner_owner
) {
    const RefusalKey key{rule.id, op, partner_owner};
    if (std::ranges::find(g_logged_refusals, key) != g_logged_refusals.end()) return;
    if (g_logged_refusals.size() < kMaxLoggedRefusals) {
        g_logged_refusals.push_back(key);
    }
    x4n::log::info(
        "sets: list {} '{}' REFUSED a partner (owner 0x{:x}) on {}",
        rule.id,
        rule.name,
        partner_owner,
        describe_operation(op)
    );
}

void vtable_destroy(Wrapper* self, int should_delete) {
    std::erase(g_wrappers, self);
    self->inner_filter.destroy();
    self->inner_filter = game::Filter();
    if (should_delete) delete self;
}

void vtable_clear_children(Wrapper* self) { self->count = 0; }

Wrapper* vtable_clone(Wrapper* self, void*) { return self; }

bool vtable_test(Wrapper* self, void* candidate) {
    if (!g_rules) return true;
    const game::Component partner(candidate);

    // Evaluate the engine's own filter first. If two list records share a filter object, unwrap one
    // extra layer so the partner is not judged by the other list's rule.
    game::Filter underlying = self->inner_filter;
    if (const Wrapper* nested = find_wrapper(underlying)) {
        if (!g_reported_nested_wrapper) {
            g_reported_nested_wrapper = true;
            x4n::log::info(
                "sets: list {} has another wrapper underneath; the engine's own filter is used",
                self->list_id
            );
        }
        underlying = nested->inner_filter;
    }
    const bool own_allows = underlying.allows(partner);

    const Rule* rule = g_rules->find(self->list_id);
    if (!rule) return own_allows;

    const Operation op = operation::consume_operation_for(self->as_filter());
    const bool active = rule->is_active();
    const std::uint64_t owner = active ? partner.owner() : 0;

    const auto permits = [&](Operation judged) {
        return decide(*rule, judged, own_allows, owner) == Decision::Allow;
    };

    const bool allowed = (op == Operation::Unknown)
        ? (permits(Operation::Sell) && permits(Operation::Buy))
        : permits(op);

    if (own_allows && !allowed && active) {
        log_refusal(*rule, op, owner);
    }
    return allowed;
}

const WrapperVtable kVtable = {
    &vtable_destroy,
    &vtable_clear_children,
    &vtable_clone,
    &vtable_test,
};

}  // namespace

void bind(const RuleTable& rules) { g_rules = &rules; }

std::int32_t list_id_of(game::Filter filter) {
    const Wrapper* wrapper = find_wrapper(filter);
    return wrapper ? wrapper->list_id : 0;
}

game::Filter wrapper_for(game::Filter filter) {
    if (!filter || find_wrapper(filter)) return filter;
    for (Wrapper* wrapper : g_wrappers) {
        if (wrapper->inner_filter == filter) return wrapper->as_filter();
    }
    return filter;
}

bool wrap(
    game::ListRecord record,
    std::int32_t list_id,
    std::int32_t trade_type
) {
    if (!record || record.type() != trade_type) return false;
    if (find_wrapper(record.filter())) return true;

    auto* wrapper = new Wrapper{};
    wrapper->vtable = &kVtable;
    wrapper->children_unowned = 1;
    wrapper->shared = 1;
    wrapper->record = record;
    wrapper->list_id = list_id;
    wrapper->inner_filter = record.filter();

    record.set_filter(wrapper->as_filter());
    g_wrappers.push_back(wrapper);
    x4n::log::info(
        "sets: list {} '{}' wrapped (its own filter {})",
        list_id,
        record.name(),
        wrapper->inner_filter ? "kept underneath" : "absent, so nothing underneath"
    );
    return true;
}

void unwrap(game::ListRecord record) {
    if (!record) return;
    Wrapper* sitting = find_wrapper(record.filter());
    if (!sitting || sitting->record != record) return;

    const std::int32_t list_id = sitting->list_id;
    const std::string name = record.name();
    sitting->restore_slot();
    std::erase(g_wrappers, sitting);
    delete sitting;
    x4n::log::info(
        "sets: list {} '{}' is no longer named; it is back as the game has it",
        list_id,
        name
    );
}

void unwrap_all() {
    for (Wrapper* wrapper : g_wrappers) {
        wrapper->restore_slot();
        delete wrapper;
    }
    g_wrappers.clear();
    g_logged_refusals.clear();
    g_reported_nested_wrapper = false;
}

}  // namespace action_based_trade_restriction::wrapper
