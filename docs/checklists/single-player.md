# Single-player checklist (manual, on retail)

The game tests can't run true single player (a dedicated server forces
deathmatch unless coop is on), so this checklist is the drop-in guarantee's
human gate. Run it with the **MSVC DLL from CI**, in retail Quake II under
Proton, at milestones M1c, M2a, M4 and M7. About 20 minutes.

## Install

1. Download the `game-module-windows-x64` artifact from the CI run of the
   commit under test.
2. In `…/steamapps/common/Quake 2/rerelease/baseq2/`, keep the current
   `game_x64.dll` (rename it, e.g. `game_x64.dll.before-check`) and copy the
   artifact's `game_x64.dll` in.
3. Start the game. The console (`~`) should show the mod's banner after
   `==== InitGame ====` once a map loads.

## Check

| # | Step | Expect |
|---|---|---|
| 1 | New game, Quake II, any skill | The intro cinematic plays, then base1 loads |
| 2 | Pick up a weapon and some ammo, kill a soldier | Normal HUD, pickup messages, weapon wheel works |
| 3 | Press F1 | The help computer shows the mission objectives |
| 4 | Quicksave (F6), walk a bit, quickload (F9) | You're back where you saved, with the same items |
| 5 | Leave base1 through the exit to base2 | The level changes; an autosave appears in the load menu |
| 6 | Load that autosave | base2 starts as it did |
| 7 | Main menu → a mission pack (The Reckoning or Ground Zero), new game | The first map loads and plays |
| 8 | Save in the mission pack, quit to menu, load it | It loads |

## Report

Tell Claude: the commit (or CI run) tested, and pass/fail per step with what
went wrong. Afterwards put the original `game_x64.dll` back.
