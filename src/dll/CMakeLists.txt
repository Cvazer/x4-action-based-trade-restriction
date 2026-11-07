cmake_minimum_required(VERSION 3.20)
project(action_based_trade_restriction LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The name of the built binary, which is the extension id read from mod.yaml.
set(X4_MOD_ID "" CACHE STRING "extension id the DLL is named for")
if(NOT X4_MOD_ID)
    message(FATAL_ERROR "X4_MOD_ID is not set; the build passes the extension id from mod.yaml")
endif()

# The SDK ships in the X4Native submodule, two levels above this project.
set(X4_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../X4Native/sdk")
if(NOT EXISTS "${X4_SDK_DIR}/x4native.h")
    message(FATAL_ERROR "no X4Native SDK at ${X4_SDK_DIR}; the submodule is not checked out")
endif()

# The UI and the DLL exchange their messages as JSON, through the same library X4Native uses.
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

file(GLOB DLL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp"
)
add_library(${X4_MOD_ID} SHARED ${DLL_SOURCES})

target_include_directories(${X4_MOD_ID} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${X4_SDK_DIR}"
)

set_target_properties(${X4_MOD_ID} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${X4_MOD_ID}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(${X4_MOD_ID} PRIVATE nlohmann_json::nlohmann_json)
