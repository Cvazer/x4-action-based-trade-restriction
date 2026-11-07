# Action Based Trade Restriction

An X4: Foundations extension that lets a trade list restrict selling and buying
separately. A list keeps its default faction set, applied to every trade, and
gains an optional sell set and an optional buy set, each with its own blacklist
or whitelist switch.

## Overview

X4's trade lists hold one set of factions, and that set governs every trade a
ship makes under the list. A list that refuses a faction refuses it both ways:
the ship can neither sell to it nor buy from it.

This mod adds two restrictions to each trade list, one for selling and one for
buying. Each is switched on by itself, holds its own factions, and either
refuses the factions it lists or lets only those through. The list's own
factions still apply to every trade, and a set only narrows what they allow: a
partner the list refuses stays refused whatever the sets say.

The sets are edited on the trade list's own page in the Player Information menu,
under **Separate sell restriction** and **Separate buy restriction**, and are
kept in the save game with the list. A list with neither set switched on behaves
exactly as the game has it. A save made with the mod and loaded without it drops
the sets, and every list keeps its own factions.

## How it works

The list's page is patched in Lua. `action_based_trade_restriction_menu.lua`
adds the two blocks to the game's trade list editor, and
`action_based_trade_restriction_editor.lua` draws each block's switches, its
factions and a picker that reuses the game's own faction panel. Confirming the
page sends the sets to the native half through X4Native.

The native half is a DLL loaded by X4Native. It places a wrapper over the
compiled filter of every trade list that carries a set. When the engine tests a
partner against the list, the wrapper asks the game's own filter first and then
narrows that verdict with the sell set or the buy set.

The engine does not tell a filter which side of a trade it is checking, so the
DLL works it out. Inside a `find_sell_offer` or `find_buy_offer` search, the
script action's command decides it for the whole search. Otherwise, when the
filter is fetched for a ship, that ship's current trade order decides it for
that check. A check whose side cannot be told is judged by both sets.

The sets are written into the save beside the trade list they belong to and read
back on load, through X4Native's save XML hooks. A faction a set names that the
game cannot resolve switches that set off, so it restricts nobody rather than
everybody.

## Building

`just` is the only entrypoint (see `Justfile`):

- `just stage` - stage the extension into `dist/`
- `just dll` - compile the DLL, `just dll Debug` for a development build
- `just build` - zip the distributable
- `just test` - run the test suite
- `just sync-versions` - recompute `versions.yaml` from the installed game
- `just schemas` / `just index` - mine schemas / index the game
- `just clean` - remove build outputs

Copy `.env.example` to `.env` and set `MOD_VERSION` and `GAME_DIR` first.

## Requirements

- 9.0.0.
- X4Native, see Dependencies.

## Dependencies

The native half runs on X4Native, which is installed on its own in
`extensions/x4native/` and is never part of this mod's archive. X4Native loads
DLLs through the UI, so protected UI mode must be off.

X4Native is at `github.com/eg3r/X4Native`. The mod needs a release that
registers what it reaches in the game:

- the trade list registry and the game's trade internals
- the ship order queue and default order accessors
- `FindOfferAction`, with the `find_sell_offer` and `find_buy_offer` command ids
- save XML name registration for extensions
