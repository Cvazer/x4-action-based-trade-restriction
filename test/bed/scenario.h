#pragma once

// A scenario is one file under `tests/`, registered by name and run by the executable's main.

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace bed {

struct Scenario {
    std::string name;
    std::function<void(const std::filesystem::path&)> body;
};

std::vector<Scenario>& scenarios();

struct Register {
    Register(const char* name, std::function<void(const std::filesystem::path&)> body);
};

void check(const char* what, bool holds);
int failures();

}  // namespace bed

#define BED_SCENARIO(name)                                                 \
    static void bed_scenario_##name(const std::filesystem::path& folder);    \
    static ::bed::Register bed_register_##name(#name, bed_scenario_##name); \
    static void bed_scenario_##name(const std::filesystem::path& folder)
