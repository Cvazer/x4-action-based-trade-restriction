#include "game/blacklist.h"

#include <x4native.h>

namespace action_based_trade_restriction::game {
namespace {

constexpr std::size_t kFilterVtableDestroy = 0;
constexpr std::size_t kFilterVtableAllows = 3;

}  // namespace

bool Filter::allows(Component partner) const {
    return !*this || method<bool (*)(void*, void*)>(kFilterVtableAllows)(object(), partner.object());
}

void Filter::destroy() const {
    if (*this) method<void (*)(void*, int)>(kFilterVtableDestroy)(object(), 1);
}

std::string ListRecord::name() const {
    return field<std::string>(x4n::detail::offsets()->blacklist_record_name);
}

std::int32_t ListRecord::type() const {
    return field<std::int32_t>(x4n::detail::offsets()->blacklist_record_type);
}

Filter ListRecord::filter() const {
    return Filter(field<void*>(x4n::detail::offsets()->blacklist_record_filter));
}

void ListRecord::set_filter(Filter filter) const {
    field<void*>(x4n::detail::offsets()->blacklist_record_filter) = filter.object();
}

namespace lists {
namespace {

void* registry() {
    auto* game = x4n::game();
    const auto* offsets = x4n::detail::offsets();
    if (!game || !offsets || !game->BlacklistRecordById) return nullptr;
    return dereference_slot(offsets->blacklist_registry);
}

}  // namespace

bool available() { return registry() != nullptr; }

std::optional<ListRecord> record(std::int32_t id) {
    void* reg = registry();
    void* found = reg ? x4n::game()->BlacklistRecordById(reg, id) : nullptr;
    if (!found) return std::nullopt;
    return ListRecord(found);
}

std::optional<std::int32_t> trade_type() {
    auto* game = x4n::game();
    if (!game || !game->BlacklistTypeFromName) {
        x4n::log::error("objectactivity: BlacklistTypeFromName did not resolve");
        return std::nullopt;
    }
    struct StringView {
        const char* data;
        std::uint64_t length;
    } constexpr kTradeTypeName{"objectactivity", 14};

    const auto type = static_cast<std::int32_t>(game->BlacklistTypeFromName(nullptr, &kTradeTypeName));
    if (type < 0 || type > 2) {
        x4n::log::error("objectactivity resolved to an unusable index {}", type);
        return std::nullopt;
    }
    x4n::log::info("objectactivity is list type {}", type);
    return type;
}

}  // namespace lists

}  // namespace action_based_trade_restriction::game
