# 0024: Classic jumps below height 0

**Decided:** 2026-10-30 (Chris: "we need to make this the default change,
at least in multiplayer - it's an exception because it brings back jumps
that were lost from OG > RE ... but cvared of course").

Original Quake II stored a player's position in eighths of a unit and
rounded it toward zero, so below height 0 a player stepped and jumped a
little higher. The rerelease moved to floats and lost that, and with it
jumps some maps were built around. MuffMode puts it back (its
`src/shared/movement.cpp`), and players on Bloody Blastard found
mm-mcoil's jumps impossible here: the whole map is below 0 (z -368 to 112).

**Decided:**
- `g_classic_jumps` (default `1`), deathmatch only: under height 0 a step
  is 20 units instead of 18 and a jump's upward speed 274 instead of 270
  (about 1.4 units higher), and a jump's speed is rounded up. MuffMode's
  numbers exactly, since those are the jumps players know. Above 0 nothing
  changes but the rounding (under a unit).
- Single player and coop stay stock (a safe drop-in): the setting is read
  only in deathmatch.
- It applies from the next map. The level tells clients in a configstring
  of the game's own (`CONFIG_CLASSIC_JUMPS`), so a client running the
  mod's own DLL predicts the same jumps.
- **An exception to hard rule 2** ("no movement changes"). A stock client
  predicts stock jumps, so below 0 it sees a small correction at the top
  of a jump and on a 19-20 unit step. MuffMode ran with that on Bloody
  Blastard and players asked for the jumps back, not about the
  correction. Hard rule 2 stands for everything else.

The numbers are core/movement.cpp (unit tests: tests/test_movement.cpp);
id's pmove calls them from four marked lines. Harness tier 16 checks the
configstring is on in an FFA.

**Reopen if:** stock clients' correction turns out to be noticeable in
play, or a map relies on stock's heights below 0.
