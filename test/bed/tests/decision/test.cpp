// The decision, given the game's verdict, the operation and the two actions.
//
// Every case is a file in `parameters/`, read when the test runs. A case supplies what a trade check
// presents to the decision and names the decision it must produce. A case is data, never an
// assertion, so a case cannot pass by returning a constant.

#include "verdict.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Faction {
    const char* name;
    std::uint64_t id;
};

constexpr Faction kFactions[] = {{"terran", 101}, {"pioneer", 202}, {"argon", 303}};

std::uint64_t faction(const std::string& name) {
    for (const Faction& known : kFactions) {
        if (name == known.name) return known.id;
    }
    return std::strtoull(name.c_str(), nullptr, 10);
}

struct Case {
    std::string name;
    std::string what;
    action_based_trade_restriction::Rule rule;
    action_based_trade_restriction::Operation operation = action_based_trade_restriction::Operation::Unknown;
    bool own_allows = true;
    std::uint64_t partner = 0;
    action_based_trade_restriction::Decision expected = action_based_trade_restriction::Decision::Allow;
};

std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// "off" or "on allow|refuse [name,name...]".
bool parse_action(const std::string& text, action_based_trade_restriction::ActionRule& out) {
    std::istringstream words(text);
    std::string state;
    words >> state;
    out = action_based_trade_restriction::ActionRule{};
    if (state == "off") return true;
    if (state != "on") return false;

    std::string mode;
    words >> mode;
    if (mode == "allow") out.whitelist = true;
    else if (mode == "refuse") out.whitelist = false;
    else return false;
    out.enabled = true;

    std::string names;
    words >> names;
    std::size_t start = 0;
    while (start <= names.size() && !names.empty()) {
        const std::size_t comma = names.find(',', start);
        const std::string one = names.substr(start, comma == std::string::npos ? std::string::npos
                                                                              : comma - start);
        if (!one.empty()) out.owners.push_back(faction(one));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return true;
}

bool load(const std::filesystem::path& file, Case& out) {
    std::ifstream input(file);
    if (!input) return false;

    out = Case{};
    out.name = file.stem().string();

    std::string line;
    while (std::getline(input, line)) {
        const std::string text = trim(line);
        if (text.empty()) continue;
        if (text[0] == '#') {
            if (out.what.empty()) out.what = trim(text.substr(1));
            continue;
        }
        const std::size_t colon = text.find(':');
        if (colon == std::string::npos) return false;
        const std::string key = trim(text.substr(0, colon));
        const std::string value = trim(text.substr(colon + 1));

        if (key == "what") out.what = value;
        else if (key == "operation") {
            if (value == "sell") out.operation = action_based_trade_restriction::Operation::Sell;
            else if (value == "buy") out.operation = action_based_trade_restriction::Operation::Buy;
            else out.operation = action_based_trade_restriction::Operation::Unknown;
        } else if (key == "own") out.own_allows = value != "deny";
        else if (key == "partner") out.partner = faction(value);
        else if (key == "expect") out.expected = value == "allow" ? action_based_trade_restriction::Decision::Allow
                                                                  : action_based_trade_restriction::Decision::Deny;
        else if (key == "sell") { if (!parse_action(value, out.rule.sell)) return false; }
        else if (key == "buy") { if (!parse_action(value, out.rule.buy)) return false; }
    }
    return !out.what.empty();
}

}  // namespace

int main() {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(BED_CASES_DIR)) {
        if (entry.path().extension() == ".case") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    int failures = 0;
    for (const auto& file : files) {
        Case item;
        if (!load(file, item)) {
            std::printf("BED FAIL %s: the case does not parse\n", file.filename().string().c_str());
            ++failures;
            continue;
        }

        const action_based_trade_restriction::Decision got =
            action_based_trade_restriction::decide(item.rule, item.operation, item.own_allows, item.partner);
        const bool holds = got == item.expected;
        if (!holds) ++failures;
        std::printf("BED %-4s %-55s %s\n", holds ? "PASS" : "FAIL", item.name.c_str(),
                    item.what.c_str());
    }

    std::printf("\n%zu case(s), %d failed\n", files.size(), failures);
    return failures == 0 ? 0 : 1;
}
