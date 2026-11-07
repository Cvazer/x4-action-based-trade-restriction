// The editor's screen, executed.
//
// Loads the Lua files the extension's `ui.xml` lists into the game's own LuaJIT, after a fake UI surface
// in place of the game's (`surface.lua`): Helper, the menu and its tables, the engine calls the scripts
// bind through ffi. What the scripts draw is recorded, and `editor_test.lua` reads it.
//
//   lua_host <lua51_64.dll> <ui.xml> <surface.lua> <editor_test.lua>
//
// Exits 0 when every check holds.

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// The engine calls the script binds with ffi.cdef. The host is the engine here.
// ---------------------------------------------------------------------------

extern "C" __declspec(dllexport) unsigned int GetAllFactions(const char** result,
                                                             unsigned int length, bool) {
    const char* factions[] = {"terran", "argon"};
    unsigned int count = 0;
    for (const char* id : factions) {
        if (count >= length) break;
        result[count++] = id;
    }
    return count;
}

extern "C" __declspec(dllexport) unsigned int GetNumAllFactions(bool) { return 2; }

// ---------------------------------------------------------------------------
// Lua
// ---------------------------------------------------------------------------

extern "C" {
typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State*);
}

namespace {

using NewState = lua_State* (*)();
using OpenLibs = void (*)(lua_State*);
using LoadFile = int (*)(lua_State*, const char*);
using LoadText = int (*)(lua_State*, const char*);
using PCall = int (*)(lua_State*, int, int, int);
using ToString = const char* (*)(lua_State*, int);
using ToNumber = double (*)(lua_State*, int);
using SetGlobal = void (*)(lua_State*, const char*);
using GetField = void (*)(lua_State*, int, const char*);
using GetGlobal = int (*)(lua_State*, const char*);
using Close = void (*)(lua_State*);

struct Runtime {
    NewState new_state = nullptr;
    OpenLibs open_libs = nullptr;
    LoadFile load_file = nullptr;
    LoadText load_text = nullptr;
    PCall p_call = nullptr;
    ToString to_string = nullptr;
    ToNumber to_number = nullptr;
    SetGlobal set_global = nullptr;
    GetField get_field = nullptr;
    GetGlobal get_global = nullptr;
    Close close = nullptr;
};

std::string error_of(Runtime& rt, lua_State* state) {
    const char* message = rt.to_string ? rt.to_string(state, -1) : nullptr;
    return message ? message : "unknown error";
}

bool run_chunk(Runtime& rt, lua_State* state, const char* what, const char* text, bool is_file) {
    const int loaded = is_file ? rt.load_file(state, text) : rt.load_text(state, text);
    if (loaded != 0) {
        std::printf("FAIL %s does not compile: %s\n", what, error_of(rt, state).c_str());
        return false;
    }
    if (rt.p_call(state, 0, 0, 0) != 0) {
        std::printf("FAIL %s raised: %s\n", what, error_of(rt, state).c_str());
        return false;
    }
    return true;
}

// The Lua files `ui.xml` loads, in its order, as paths beside it: the game reads the same list.
std::vector<std::filesystem::path> shipped_scripts(const std::filesystem::path& ui_xml) {
    std::ifstream input(ui_xml);
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::regex file_entry(R"re(<file\s+name="([^"]+)")re");

    std::vector<std::filesystem::path> scripts;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), file_entry);
         it != std::sregex_iterator(); ++it) {
        scripts.push_back(ui_xml.parent_path() / (*it)[1].str());
    }
    return scripts;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::printf("usage: lua_host <lua51_64.dll> <ui.xml> <surface.lua> <editor_test.lua>\n");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        std::printf("FAIL could not load %s\n", argv[1]);
        return 2;
    }

    Runtime rt;
    rt.new_state = reinterpret_cast<NewState>(GetProcAddress(module, "luaL_newstate"));
    rt.open_libs = reinterpret_cast<OpenLibs>(GetProcAddress(module, "luaL_openlibs"));
    rt.load_file = reinterpret_cast<LoadFile>(GetProcAddress(module, "luaL_loadfile"));
    rt.load_text = reinterpret_cast<LoadText>(GetProcAddress(module, "luaL_loadstring"));
    rt.p_call = reinterpret_cast<PCall>(GetProcAddress(module, "lua_pcall"));
    rt.to_string = reinterpret_cast<ToString>(GetProcAddress(module, "lua_tolstring"));
    rt.to_number = reinterpret_cast<ToNumber>(GetProcAddress(module, "lua_tonumber"));
    rt.set_global = reinterpret_cast<SetGlobal>(GetProcAddress(module, "lua_setglobal"));
    rt.get_field = reinterpret_cast<GetField>(GetProcAddress(module, "lua_getfield"));
    rt.get_global = reinterpret_cast<GetGlobal>(GetProcAddress(module, "lua_getglobal"));
    rt.close = reinterpret_cast<Close>(GetProcAddress(module, "lua_close"));

    if (!rt.new_state || !rt.open_libs || !rt.load_file || !rt.load_text || !rt.p_call ||
        !rt.to_string || !rt.close) {
        std::printf("FAIL the library does not export the Lua C API\n");
        return 2;
    }

    lua_State* state = rt.new_state();
    rt.open_libs(state);

    const std::vector<std::filesystem::path> scripts = shipped_scripts(argv[2]);
    if (scripts.empty()) {
        std::printf("FAIL %s lists no Lua file\n", argv[2]);
        return 1;
    }

    if (!run_chunk(rt, state, "the fake surface", argv[3], true)) return 1;
    for (const std::filesystem::path& script : scripts) {
        if (!run_chunk(rt, state, script.string().c_str(), script.string().c_str(), true)) return 1;
    }
    if (!run_chunk(rt, state, "the checks", argv[4], true)) return 1;

    // The assertion count is the process result: a screen that fails a claim must fail the run.
    double failures = 0.0;
    if (rt.get_field && rt.to_number) {
        rt.get_field(state, -10002, "FAILURES");
        failures = rt.to_number(state, -1);
    }

    rt.close(state);
    return failures > 0.0 ? 1 : 0;
}
