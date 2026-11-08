# Release checklist (manual, on retail)

Only the things the game tests can't see go here. The harness already proves
the logic on every PR: saves, match flow, Duel rotation, votes, team locks,
timeouts, admin, callouts, map rotation, hostile input (AGENTS.md, tiers
1-30), and players on Bloody Blastard have judged how it looks and feels in
retail clients. What neither reaches is **the Windows DLL inside the game
itself**: single player and listen servers never go through a dedicated
server. Run it before each release. About 45 minutes.

Use the build being released: the `game-module-windows-x64` artifact from
main's CI run (`gh run download <run> -n game-module-windows-x64`), and the
same commit on the playtest rig (`~/q2playtest/update.sh` after building).

## A. Single player: the Windows DLL as a drop-in

In `…/Quake 2/rerelease/baseq2/`, rename the current `game_x64.dll`
(`game_x64.dll.before-check`) and copy the artifact's in. Make one save with
the stock DLL first, for row 8.

| # | Try | Expect |
|---|---|---|
| 1 | Load any map, open the console | The mod's banner after `==== InitGame ====` |
| 2 | New game, Quake II | Intro plays, base1 loads; fight, pick up, weapon wheel, HUD all stock |
| 3 | F1 | The help computer shows the objectives |
| 4 | Quicksave, walk, quickload | Back where you saved, same items |
| 5 | Exit base1 to base2, then load the autosave | Level changes; the autosave loads |
| 6 | The Reckoning and Ground Zero: new game, save, quit to menu, load | Each loads and plays |
| 7 | Quake 64 and Call of the Machine: start each | First map loads and plays |
| 8 | Load the save you made with the stock DLL | It loads |
| 9 | Optional: split-screen coop (needs a controller) | Both players play; level change works |

## B. Multiplayer: the Windows DLL hosting

Still with the mod's DLL installed: Multiplayer → host a game.

| # | Try | Expect |
|---|---|---|
| 10 | Host Deathmatch on q2dm1 with a few bots | Banner; you arrive spectating with the menu open; join; the bots fight; nothing counts in warmup |
| 11 | `ready` | Countdown, FIGHT!, map resets, scores 0; the match ends at the timelimit, intermission, next map |
| 12 | Console: `g_gametype tdm`, `map q2dm1`; then `ctf` on q2ctf1 | Team menu, team skins, team HUD; CTF flags and capture on the HUD |
| 13 | Host Cooperative, play a few minutes | Stock coop, nothing from the mod |
| 14 | Optional, from a second machine: the server browser | Your server shows its mode (Deathmatch, Team, CTF) |

## C. What Bloody Blastard hasn't covered

Players on BB (Q2RED, the Linux module, retail clients) have played the
menus, scoreboards, HUD, votes, team locks, timeouts, callouts, classic
jumps and teleporters for weeks, so those need no rows here. Check only
what has merged since BB's build (`git log <BB's commit>..main`), in a
retail client against the rig or on BB once it's deployed. For this
release (BB on `3cd6faf`):

| # | Try | Look for |
|---|---|---|
| 15 | `cmd.sh "set admin_password test"`; in game `admin test`, then `admin` | The list is readable; replies come to you only |
| 16 | `admin readyall`, `admin lockteam`, `admin map q2dm3` | Each works; still an admin on the new map |
| 17 | Three wrong passwords | Locked out, told so |

## Report

Results go in `docs/checklists/results/<date>-release.md` (the rows, pass or
fail, with notes), so this file stays a template. Tell Claude the commit
tested and anything that felt wrong; afterwards put the original
`game_x64.dll` back.
