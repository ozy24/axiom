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
