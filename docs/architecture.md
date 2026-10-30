# Architecture

How the pieces fit, as built. Update this when a layer gains something. The
reasons behind each shape are in `docs/decisions/`.

## Layers

```
engine (retail KEX / Q2REX / Q2RED)
   │  GetGameAPI / GetCGameAPI, game_import_t (gi) — fixed ABI, never changed
   ▼
rerelease/        id's game code, edited in place (decision 0002).
                  Every edit is marked // [mod] and is ideally a one-line call
                  into core/. Single player, coop and the mission packs run
                  through this code unchanged.
   │  calls
   ▼
core/             the mod's own code. Two kinds of file:
                  • logic: no g_local.h, talks to the engine only through Host
                    (core/host.h), unit-tested with tests/fake_host.h
                  • adapters (*_register.cpp, *_gi.cpp): the only core files
                    that touch gi/level. scripts/check_core_seam.py enforces it.
gametypes/<name>/ (from Phase 2) each gametype as a module on the gametype API.
platform/posix/   the Linux entry-point shim, compat header and export map.
checks/           compile-time checks built into the module.
```

## What exists so far

| Piece | Files | What it does |
|---|---|---|
| Identity | `mod.json`, `cmake/identity.cmake`, `core/identity*.cpp` | Name, id, version and build id from one file; `gamename`, `g_version`, banner, `mod-identity:` stamp |
| Gametype registry | `core/gametype*.cpp`, `gametypes/<id>/gametype.cpp`, generated `gametype_registry.cpp` | `g_gametype` selects a registry gametype (today `ffa`) and fills Rogue's `DMGame`; stock `gamerules` still runs Rogue's modes (`docs/design/gametype-api.md`) |
| Match flow | `core/match.cpp` (state machine), `core/match_register.cpp` (adapter, arena reset, `ready`/`notready`) | Warmup → countdown → live → overtime → intermission, off by default (`match_flow 0` = stock) (`docs/design/match-flow.md`) |
| Match records | `core/match_record.cpp` (JSON, pure), `WriteMatchRecord` in `core/match_register.cpp` | `g_matchlog_dir`: one JSON file per finished match; a relative directory goes under the game directory when the engine offers file access |
| Host file access | `core/host_files.cpp` (game-path rule, pure), `core/host_files_gi.cpp`, `core/host_files_ext.h` | Q2RED's optional `GetExtension` file access, asked for once per load; callers always have a fallback (decision 0011) |
| Duel | `gametypes/duel/` (`duel_queue.cpp` pure, `duel_register.cpp` adapter) | 1v1 play queue, forfeit, winner stays on; `queue` command |
| TDM, CTF | `gametypes/tdm/`, `gametypes/ctf/` (capturelimit), `Core_Gametype_PreInit` in `core/gametype_register.cpp` | Registry gametypes on stock's team code: `g_gametype` sets the stock `teamplay` / `ctf` cvars (decision 0012); the match flow scores by team |
| Rulesets | `core/rulesets*.cpp` | a gametype's gameplay settings and its variants (decision 0013): built-ins `ffa`, `duel`, `tdm`, `ctf`; `rulesets/<gametype>.cfg` changes one; `rulesets/<gametype>_<name>.cfg` listed in `g_ruleset_variants` adds a variant |
| HUD stat slots | `core/stat_slots*.cpp` | Hands out stat indices 54..63 (stock's free range) by owner, idempotently; the CTF block stays stock's |
| Host seam | `core/host.h`, `core/host_gi.cpp`, `tests/fake_host.h` | The engine interface logic uses: `Print`, `TimeMs`. Grow it only when a module needs more |
| Stock-client contract | `checks/stock_client_contract.cpp` | Pins stat, ammo and powerup indices to stock |

## Build outputs

- `game_x64.dll` (MSVC, the only build that ships) and `game_x64.so` (GCC,
  for Q2RED). Same sources; `platform/posix/` only on Linux; cgame only on
  Windows for now.
- `game_tests`: doctest unit tests over core logic (no engine), run by ctest
  in both CI builds.

## How a change is verified

1. `scripts/check.sh`: hygiene, save baseline, name leak, `[mod]` markers,
   core seam, format. Hosted CI also runs both builds, the unit tests and the
   rename drill.
2. The jumpbox runner runs the game tests against the stock reference: tiers
   1-3 on Q2RED with the `.so` and on Q2REX under Wine with the MSVC `.dll`.
3. Human gates (`STATUS.md`): the single-player checklist on retail
   (`docs/checklists/single-player.md`) and playtests of new gameplay
   (`docs/checklists/playtest.md`, with the local rig from
   `scripts/playtest/setup-local-rig.sh`).

## Coming next (plan Phase 2)

Predicates and hooks replacing scattered mode checks in `rerelease/`, the
gametype API (successor to Rogue's `DMGame`), the match state machine,
rulesets as `.cfg`, the team service and the HUD stat allocator.
