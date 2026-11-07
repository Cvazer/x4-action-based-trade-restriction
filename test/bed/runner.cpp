#include "common.h"
#include "scenario.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <process.h>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

// The scratch folders sit beside this binary, inside the bed's own build tree, so a run started from
// any working directory leaves the project root untouched.
std::filesystem::path scratch_root() {
    wchar_t path[MAX_PATH];
    const DWORD written = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (written == 0 || written == MAX_PATH) {
        return std::filesystem::temp_directory_path() / "bed-scratch";
    }
    return std::filesystem::path(path).parent_path() / "bed-scratch";
}

// One scenario runs in a process of its own: the mod keeps global state between a load and an unload,
// and a scenario that ends badly must not take the rest of the suite with it.
int run_one(const bed::Scenario& scenario) {
    const std::filesystem::path folder = scratch_root() / scenario.name;
    std::filesystem::create_directories(folder);
    scenario.body(folder);
    return bed::failures() == 0 ? 0 : 1;
}

int run_all(const std::filesystem::path& self, const std::vector<bed::Scenario>& all) {
    int failed_scenarios = 0;
    for (const bed::Scenario& scenario : all) {
        std::printf("\n--- %s\n", scenario.name.c_str());
        const std::wstring executable = self.wstring();
        const std::wstring name(scenario.name.begin(), scenario.name.end());
        std::vector<const wchar_t*> argv;
        argv.push_back(executable.c_str());
        argv.push_back(L"--only");
        argv.push_back(name.c_str());
        argv.push_back(nullptr);
        const intptr_t code = _wspawnv(_P_WAIT, executable.c_str(), argv.data());
        if (code != 0) ++failed_scenarios;
    }
    return failed_scenarios;
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::vector<bed::Scenario> all = bed::scenarios();
    std::sort(all.begin(), all.end(),
              [](const bed::Scenario& left, const bed::Scenario& right) {
                  return left.name < right.name;
              });

    if (argc == 3 && std::strcmp(argv[1], "--only") == 0) {
        for (const bed::Scenario& scenario : all) {
            if (scenario.name == argv[2]) return run_one(scenario);
        }
        std::printf("no scenario named %s\n", argv[2]);
        return 1;
    }

    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const int failed = run_all(self, all);

    std::printf("\n%zu scenario(s), %d failed\n", all.size(), failed);
    return failed == 0 ? 0 : 1;
}
