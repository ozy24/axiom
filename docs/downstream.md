# Building a mod on Axiom

Axiom is meant to be built on. A downstream mod (Jump, Rocket Arena, Freeze
Tag, Horde) keeps Axiom as a git submodule and adds its own gametypes and
identity, **without changing Axiom's code**. It gets everything Axiom has:
the match flow, votes, rulesets, the Tab menu, team locks, timeouts,
in-game admin, location callouts, the HUD and scoreboards, the stock-client
contract checks, and a module that stays a safe drop-in for single player.

`examples/jump/` is a complete one, built and run in CI on every pull
request (the hosted `downstream` and `windows` jobs; harness tier 30 on the
jumpbox). Copy it to start.

**Status: v0.** The surface below is what downstream mods build against.
Until 1.0 it may still change; every change is listed in the changelog.

## A mod's layout

```
my-mod/
  mod.json               your name, id, version, repository
  CMakeLists.txt         four settings, then add_subdirectory(axiom)
  axiom/                 git submodule (this repository)
  gametypes/<id>/        one folder per gametype of your own
    gametype.cpp         its GametypeDef, named gametype_<id>
    *.cpp                every .cpp in the folder is built in
  tests/                 your unit tests
```

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_mod LANGUAGES CXX)

set(MOD_IDENTITY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mod.json")
set(GAME_GAMETYPE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/gametypes")
set(GAME_GAMETYPES ffa duel tdm ctf freeze)
set(GAME_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(axiom base)
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build          # build/base/game_x64.so
```

On Windows use MSVC (the compiler retail is built with), from a Visual
Studio developer prompt: add `-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl`
and you get `build/base/game_x64.dll`. Ship only MSVC builds to players.

## The build settings

| Setting | What it does |
|---|---|
| `MOD_IDENTITY_FILE` | Your `mod.json`. The module reports your name and version (`g_version`, the banner, `gamename`), and the build id is your repository's commit. Never hard-code your name: take it from `mod_identity.h` (`MOD_NAME`, `MOD_ID`). |
| `GAME_GAMETYPES` | The gametypes registered, in this order. `ffa` must stay: an unknown `g_gametype` falls back to it. Drop Axiom's others if your mod doesn't want them. |
| `GAME_GAMETYPE_DIRS` | Folders searched for `<id>/gametype.cpp` before Axiom's own `gametypes/`. A folder of yours with the same id as one of Axiom's replaces it. |
| `GAME_INCLUDE_DIRS` | Extra include paths for your code. |
| `GAME_BUILD_TESTS` | Axiom's own unit tests. Off when Axiom is a subdirectory; build your own. |

## Writing a gametype

The full reference is [the gametype API](design/gametype-api.md). In short,
a gametype is one `GametypeDef` (`core/gametype.h`):

- `id`, `title`, and `retail_mode`: the stock mode it runs on and shows in
  retail's server browser (`deathmatch`, `teamplay` or `ctf`). A team
  gametype runs on stock's team code.
- `install(dm_game_rt &)`: fills id's own dispatch table (`DMGame`),
  which id's code already calls at the right places: `ChangeDamage`,
  `ChangeKnockback`, `ClientBegin`, `SelectSpawnPoint`, `PlayerDeath`,
  `Score` and the rest. It's reset before each gametype installs.
- The match flow's hooks: `frame`, `match_over` (your end condition),
  `match_ended`, `client_command` (your commands), `waiting` (a queue like
  Duel's) and `scoreboard`.

Every hook is optional; null means stock behaviour. New hooks are only
ever **added at the end** of `GametypeDef`, so a definition written today
keeps compiling: the members it leaves out are null.

Split each gametype as Axiom does: pure logic in plain `.cpp` files, with
no `g_local.h`, that unit tests can link; and an adapter (`*_register.cpp`)
that touches `gi`, `level` and edicts. Keep per-player data in your own
module, indexed by client slot, not in id's structs: those are saved, and
the save contract below applies.

Players get your gametype with `g_gametype <id>` (latched: it applies at the
next `map`) or a gametype vote. Its settings come from an admin's
`rulesets/<id>.cfg`, exec'd after `set g_gametype <id>`; see
[docs/admin.md](admin.md).

## Rules you inherit

These come from the engine and retail clients, not taste. The first is
checked every time your module compiles.

- **Stock clients run their own client game.** No movement changes (they
  predict movement), no new stats beyond the allocator's free slots
  (`core/stat_slots.h`), no new ammo or powerup types (bit-packed by
  index), no new `$loc` keys (they show raw). `checks/stock_client_contract.cpp`
  fails the build if a stat or index moves.
- **Saves.** Never rename or remove a saved field or a registered function
  name in id's code; adding is fine.
- **A safe drop-in.** Players install by overwriting `baseq2/game_x64.dll`:
  keep single player, coop and the mission packs working as stock. Gate
  your code on `deathmatch` and your gametype.
- **Neutral public names.** Cvars and commands use `g_*`, `match_*`,
  `sv_*`, never your mod's name.
- **Chat is not a command channel.** Retail sends chat through the lobby;
  take commands as console commands (`client_command`).

## What isn't offered (yet)

- **Engine hooks beyond `DMGame` and `GametypeDef`.** If your gametype
  needs a call from a place id's code doesn't reach (death interception for
  Freeze Tag, monster spawning for Horde), open an issue: hooks are added
  as one-line calls in id's code when a gametype first needs them, never
  speculatively.
- **A loadable plugin.** A mod is compiled with Axiom into one module, the
  only kind the engine loads.
