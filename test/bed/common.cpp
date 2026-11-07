#include "common.h"

#include "events.h"

#include <nlohmann/json.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace bed {

Fixture start(const std::filesystem::path& folder) {
    bed::boot((folder / "").string() + "\\");

    Fixture fixture;
    fixture.terran = bed::add_faction("terran");
    fixture.argon = bed::add_faction("argon");
    x4native_init(&bed::engine().api);

    fixture.engine_filter = bed::make_engine_filter();
    fixture.record = bed::make_record(1, "Scoped Sell", fixture.engine_filter);
    return fixture;
}

void* filter_slot(void* record) {
    return bed::at<void*>(record, bed::engine().offsets.blacklist_record_filter);
}

bool wrapper_allows(void* wrapper, void* partner) {
    void** vtable = bed::at<void**>(wrapper, 0);
    auto test = reinterpret_cast<bool (*)(void*, void*)>(vtable[3]);
    return test(wrapper, partner);
}

namespace {

std::optional<bool> search_allows(std::uint32_t command, void* wrapper, void* partner) {
    bool allowed = false;
    if (!bed::run_find_offer(bed::make_script_action(command),
                             [&] { allowed = wrapper_allows(wrapper, partner); }))
        return std::nullopt;
    return allowed;
}

}  // namespace

std::optional<bool> sale_allows(void* wrapper, void* partner) {
    return search_allows(bed::kFindBuyOffer, wrapper, partner);
}

std::optional<bool> purchase_allows(void* wrapper, void* partner) {
    return search_allows(bed::kFindSellOffer, wrapper, partner);
}

bool build_safely(void* record) {
    auto* build = reinterpret_cast<void* (*)(void*)>(bed::detour_for("BlacklistFilterBuild"));
    if (!build) return false;
    __try {
        build(record);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

namespace {

nlohmann::ordered_json shorthand_set(const std::string& text) {
    // "sell:10:terran,argon"
    const std::size_t first = text.find(':');
    const std::size_t second = text.find(':', first + 1);
    const std::string flags = text.substr(first + 1, second - first - 1);
    nlohmann::ordered_json factions = nlohmann::ordered_json::array();
    const std::string list = text.substr(second + 1);
    std::size_t start = 0;
    while (start < list.size()) {
        const std::size_t end = list.find(',', start);
        factions.push_back(list.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {{"enabled", flags[0] == '1'}, {"whitelist", flags[1] == '1'}, {"factions", factions}};
}

std::string shorthand_of(const nlohmann::json& entry) {
    std::string text = std::to_string(entry.at("list").get<int>());
    for (const char* which : {"sell", "buy"}) {
        const nlohmann::json& set = entry.at(which);
        text += std::string("|") + which + ":" + (set.at("enabled").get<bool>() ? "1" : "0") +
                (set.at("whitelist").get<bool>() ? "1" : "0") + ":";
        bool first = true;
        for (const nlohmann::json& faction : set.at("factions")) {
            if (!first) text.push_back(',');
            text += faction.get<std::string>();
            first = false;
        }
    }
    return text;
}

}  // namespace

void send(const char* entry) {
    const std::string text = entry;
    nlohmann::ordered_json message;
    message["list"] = std::stoi(text.substr(0, text.find('|')));
    std::size_t start = text.find('|');
    while (start != std::string::npos) {
        const std::size_t end = text.find('|', start + 1);
        const std::string part = text.substr(start + 1, end == std::string::npos ? end : end - start - 1);
        message[part.substr(0, part.find(':'))] = shorthand_set(part);
        start = end;
    }
    send_raw(message.dump().c_str());
}

void send_raw(const char* message) { bed::raise(bed::events::set, const_cast<char*>(message)); }

std::string notation(const std::string& message) {
    const nlohmann::json value = nlohmann::json::parse(message, nullptr, false);
    if (value.is_discarded()) return {};
    try {
        if (value.is_object()) return shorthand_of(value);
        std::string text;
        for (const nlohmann::json& entry : value) {
            if (!text.empty()) text.push_back(';');
            text += shorthand_of(entry);
        }
        return text;
    } catch (const nlohmann::json::exception&) {
        return {};
    }
}

void* save_game() {
    void* lists = bed::make_save_node(0);
    bed::run_list_export(lists);
    return lists;
}

void load_game(void* saved) {
    bed::run_list_import(saved);
    bed::raise("on_game_loaded");
}

}  // namespace bed
