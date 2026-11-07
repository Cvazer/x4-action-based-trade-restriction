#pragma once

// The names of the events the mod's layers exchange, as the tests raise and read them. One place, so
// the tests follow the mod's own names rather than repeating them.

namespace bed::events {

inline constexpr const char* set = "action_based_trade_restriction.set";          // Lua -> DLL: one list's entry
inline constexpr const char* request = "action_based_trade_restriction.request";  // Lua -> DLL: report the table
inline constexpr const char* sets = "action_based_trade_restriction.sets";        // DLL -> Lua: the table it holds
inline constexpr const char* arm = "action_based_trade_restriction.arm";          // DLL -> Lua: the table is filled

}  // namespace bed::events
