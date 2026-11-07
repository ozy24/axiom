# 0028: Downstream mods build Axiom as a subdirectory

**Decided:** 2026-11-07 (Chris: get the release solid, "especially if
there's an api ... for the downstream mods"). Phase 6 of the plan.

A downstream mod keeps Axiom as a git submodule and builds it with
`add_subdirectory()`, setting four variables first: `MOD_IDENTITY_FILE`
(its `mod.json`), `GAME_GAMETYPES`, `GAME_GAMETYPE_DIRS` (its own gametype
folders, searched before Axiom's) and `GAME_INCLUDE_DIRS`. Nothing in
Axiom is copied or edited. docs/downstream.md is the guide.

**Why this way:**
- One module is all the engine loads, so a mod is compiled with Axiom.
  A plugin loader stays possible (decision 0005) but nothing needs it.
- The gametype registry was already generated from a CMake list; letting
  the list's folders live outside the tree was the smallest change.
- An OBJECT library of "core" was the plan's other option. It would split
  id's code from Axiom's, which can't be built apart (Axiom's hooks are
  calls inside id's files), for no gain over a subdirectory.

**What changed in Axiom:** paths in CMake are the project's own
(`CMAKE_CURRENT_SOURCE_DIR`), not the top level's; the build id comes
from the git repository holding the identity file; Axiom's unit tests
build only when it is the top-level project; `DMGame` is cleared before a
gametype installs; and `T_Damage`'s `ChangeDamage` / `ChangeKnockback`
gate admits registry gametypes (identical to stock with no hook).

**Kept honest by:** `examples/jump`, a real downstream mod (its own
identity, a `jump` gametype with no damage from players or falls, a run
timer, its own unit tests). CI builds it with GCC and MSVC (`downstream`,
`windows`), and harness tier 30 boots it with bots: they frag under ffa
and never under jump.

**Stability:** v0 until 1.0. `GametypeDef` only grows at the end, so
existing definitions keep compiling.

**Reopen if:** a downstream mod needs to add code outside a gametype folder
(shared libraries of its own, new console commands not tied to a gametype),
or to replace one of Axiom's core systems.
