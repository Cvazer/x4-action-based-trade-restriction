# The bed's own build. Nothing here comes from the mod's harness: no game, no staging.
#
#   cmake -S test/bed -B test/bed/build
#   cmake --build test/bed/build --config Release
#   ctest --test-dir test/bed/build -C Release --output-on-failure
#
# Every test under `tests/` is one mechanism. The decision is its own executable; the rest drive the
# mod's own translation units against the fake engine and run from one binary.

cmake_minimum_required(VERSION 3.20)
project(action_based_trade_restriction_testbed LANGUAGES CXX)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The UI and the DLL exchange their messages as JSON, through the same library X4Native uses.
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# The mod's sources the tests compile: the decision alone, and every unit of the DLL.
set(DECISION_SOURCE "${PROJECT_ROOT}/src/dll/domain/verdict.cpp")
file(GLOB DLL_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT}/src/dll/*.cpp"
    "${PROJECT_ROOT}/src/dll/*/*.cpp"
)

# The decision, with one file per variation under `parameters/`.
add_executable(bed
    tests/decision/test.cpp
    ${DECISION_SOURCE})

target_include_directories(bed PRIVATE
    ${PROJECT_ROOT}/src/dll
    ${PROJECT_ROOT}/src/dll/domain
)
target_compile_definitions(bed PRIVATE
    BED_CASES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/decision/parameters")

# Everything that drives the mod's own code against a fake engine.
# X4Native's own helpers are tested under `tests/x4native/`, one folder per helper, and the bed builds
# without that folder.
file(GLOB ENGINE_TESTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*/test.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/x4native/*/test.cpp")
list(FILTER ENGINE_TESTS EXCLUDE REGEX "/tests/decision/")

# X4Native's save XML name registry, which the fake engine runs as the framework does.
set(X4NATIVE_CORE_SOURCES "${PROJECT_ROOT}/X4Native/src/core/save_xml.cpp")

add_executable(integration
    runner.cpp
    scenario.cpp
    common.cpp
    harness/fake_engine.cpp
    ${ENGINE_TESTS}
    ${DLL_SOURCES}
    ${X4NATIVE_CORE_SOURCES})

target_include_directories(integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/harness
    ${PROJECT_ROOT}/src/dll
    ${PROJECT_ROOT}/X4Native/sdk
    ${PROJECT_ROOT}/X4Native/src/core)

# The editor's Lua runs on the game's own LuaJIT: the bed cannot drive the UI, but it can execute the
# script against a fake surface and refuse one built wrong.
add_executable(lua_host tests/editor/host.cpp)

if(MSVC)
    target_compile_options(bed PRIVATE /W4 /permissive-)
else()
    target_compile_options(bed PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME decision COMMAND bed)
add_test(NAME integration COMMAND integration)

target_link_libraries(integration PRIVATE nlohmann_json::nlohmann_json)
