# Joining

Status: done (decision 0017). Change this page in the same PR as the code.

A human new to the server arrives **spectating, with the Tab menu open**, and
chooses to play. Players already playing stay playing from map to map. Pure
logic: `core/join.{h,cpp}` (tests: `tests/test_join.cpp`); engine side:
`core/join_register.cpp`; harness: tier 18.

## Arriving

| Who | Arrives |
|---|---|
| A human new to the server | spectating, menu open (team modes: on no team, the same menu) |
| Someone playing on the last level (back within 30 s of the new one) | as they were: playing, on the same team; in Duel, in the queue |
| A bot | straight into play (stock's team assignment in team modes) |
| The host of a listen server | straight into play (the smaller team) |
| Anyone, with `g_auto_join 1` | straight into play (the smaller team) |

`g_auto_join` (default `0`) is a server setting, for `server.cfg`; rulesets
don't touch it. `g_teamplay_force_join 1` still puts everyone on a team, as
stock.

## Commands

| Command | FFA | TDM, CTF | Duel |
|---|---|---|---|
| `join` | into play | the smaller team you may join | into the queue |
| `spectate` | out of play | off your team | out of the queue (out of a duel: a forfeit) |

The menu's Join the Match / Join Queue / Leave Queue / Spectate rows run
the same code. `team red` / `team blue` still pick a team.

## Across map changes

Under KEX every human reconnects at each map change, so arriving happens
every level. The players of the last level are remembered by social ID
(`Core_PlayerKey`) and come back as they were for the first 30 s of the
new level; after that the memory is of this level's players, and someone
reconnecting mid-level arrives like anyone new.

`map` (votes, an admin) reloads the game module, so the memory lives in a
cvar, `g_join_memory` (internal: write-protected, only the module sets it): `<hash>:<team>` per player, each key an 8-digit
FNV-1a hash, written only when it changes. It is the game's own state, not
a setting. Anything malformed in it is skipped.

## Duel's queue

Only those who asked to play are queued: `join` or the menu, bots, the last
level's players and, with `g_auto_join 1`, everyone. Someone who only wants
to watch is never pulled in. The gametype's `waiting` hook tells the menu
(Join Queue or Leave Queue) and the memory who asked.

## Compared with MMREX

MMREX's arrival is the same (spectating in free cam, the join menu a moment
later; `g_dm_auto_join`, `g_owner_auto_join`). Its players reconnecting at a
map change are kept by its ghost/session machinery; Axiom keeps the one
cvar. No message of the day screen before the menu.
