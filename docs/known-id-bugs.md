# Known bugs in id's code

Bugs in the stock rerelease game code that Axiom fixes on purpose. A fix makes
Axiom differ from the stock reference, so each one needs:

1. an entry here: what the bug is, where it was found (MuffMode, Q2RED, our own
   testing), and the commit that fixes it;
2. a test that reproduces it;
3. if it changes saved state, a line in `docs/baselines/allowed-differences.txt`
   pointing back here.

Without all three, the comparison tests report the fix as a regression.

## 1. Tag crashes on the first frame of every map

- **Where:** `g_items.cpp`, `droptofloor`. `SpawnItem` sets
  `ent->classname = item->classname`, and the tag token's `itemlist` entry
  (`IT_ITEM_TAG_TOKEN`) has a `nullptr` classname. The rerelease's foodcube
  check then calls `strcmp(nullptr, "item_foodcube")`.
- **Seen:** id's source at `8dc1fc9`, `gamerules 2` (Tag) on q2dm1, on Q2RED:
  SIGSEGV in `__strcmp_avx2` from `droptofloor` (g_items.cpp:1126), with the
  stock module too. Found 2026-10-26 while (briefly) making Tag selectable by
  `g_gametype`. Whether the 2025 retail DLL still has it is not checked.
- **Fix:** null-check `ent->classname` before the comparison (`[mod]`).
- **Test:** harness tier 4 (gametype-boot) boots stock `gamerules 2` (Tag).
- **Saved state:** unchanged (no allowed-differences entry needed); the
  coop save comparison never reaches Tag.

## 2. The water-jump probe hangs the server at zero or negative gravity

- **Where:** `p_move.cpp`, `PM_CheckWaterJump`: the probe runs
  `min(50, (int32_t) (10 * (800.f / pm->s.gravity)))` steps with a `size_t`
  counter. With negative gravity the count is negative and, compared as
  `size_t`, becomes billions of steps (each a slide move with traces); at
  zero gravity it casts infinity to `int`, which is undefined.
  `pm->s.gravity` comes from `g_gravity` times the player's own gravity
  (`trigger_gravity`, `target_gravity`), so a map or an admin can reach it.
- **Seen:** found in MMREX's copy of the same code (its `shared/movement.cpp`
  carries the fix, "map data can drive gravity negative") while building a
  simulator on Axiom's pmove, 2026-10-31. Not reproduced in a running server.
- **Fix:** `Core_Movement_WaterJumpSteps` (`core/movement.cpp`): stock's count
  for positive gravity, unchanged; the magnitude for negative gravity, as
  MMREX; the cap of 50 at zero (`[mod]`, one hook line in `p_move.cpp`).
  Prediction: a stock client still runs stock's count, so the two differ only
  at gravity <= 0, where stock's own result is a hang or undefined.
- **Test:** `tests/test_movement.cpp`, "water-jump probe".
- **Saved state:** unchanged (the step count is not saved).
