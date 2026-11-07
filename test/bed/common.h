#pragma once

// What every scenario shares: a fresh engine with the mod loaded, and the few operations a scenario
// needs to read the engine's side of a call.

#include "fake_engine.h"

#include <filesystem>
#include <optional>
#include <string>

extern "C" int x4native_init(X4NativeAPI*);
extern "C" void x4native_shutdown(void);

namespace bed {

struct Fixture {
    void* engine_filter = nullptr;  // the filter the engine built for the list
    void* record = nullptr;         // that list's record, id 1, named "Scoped Sell"
    void* terran = nullptr;
    void* argon = nullptr;
};

// A fresh engine in `folder`, the two factions registered, the mod loaded, one list and nothing wrapped.
Fixture start(const std::filesystem::path& folder);

void* filter_slot(void* record);
bool wrapper_allows(void* wrapper, void* partner);

// A check of `partner` on `wrapper` inside a find_buy_offer search, where the ship sells, or a
// find_sell_offer search, where it buys. None when nothing hooked the find-offer handler.
std::optional<bool> sale_allows(void* wrapper, void* partner);
std::optional<bool> purchase_allows(void* wrapper, void* partner);
bool build_safely(void* record);

// The Lua side hands the DLL one list's entry, written in the bed's shorthand
// `<id>|sell:<enabled><whitelist>:<faction>,...|buy:...` with the flags one digit each, and sent as the
// JSON message the UI sends.
void send(const char* entry);

// The Lua side hands the DLL a message exactly as given.
void send_raw(const char* message);

// A table or an entry the DLL reported, in the bed's shorthand, entries joined by ';'. Empty for text
// that is not one.
std::string notation(const std::string& message);

// The engine saves the game: the list writer runs, the mod's hook with it. Answers the node the lists
// were written under, which is what a load of that save reads.
void* save_game();

// The engine loads a save: the list reader runs over the lists `saved` holds, then the world is loaded.
void load_game(void* saved);

}  // namespace bed
