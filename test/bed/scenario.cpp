#include "scenario.h"

#include <cstdio>

namespace bed {

namespace {
int g_failures = 0;
}

std::vector<Scenario>& scenarios() {
    static std::vector<Scenario> all;
    return all;
}

Register::Register(const char* name, std::function<void(const std::filesystem::path&)> body) {
    scenarios().push_back(Scenario{name, std::move(body)});
}

void check(const char* what, bool holds) {
    if (!holds) ++g_failures;
    std::printf("BED %-4s %s\n", holds ? "PASS" : "FAIL", what);
}

int failures() { return g_failures; }

}  // namespace bed
