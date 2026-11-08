# Release checklist (manual, on retail)

Only the things the game tests can't see go here. The harness already proves
the logic on every PR: saves, match flow, Duel rotation, votes, team locks,
timeouts, admin, callouts, map rotation, hostile input (AGENTS.md, tiers
1-30). What it can't judge is the **retail client**: how layouts render in
its fonts, how the Windows DLL behaves inside the game itself (single player
and listen servers never go through a dedicated server), and how things
feel. Run it before each release. About an hour; parts A and B are the gate,
part C is quicker with a second player.

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

## C. Retail client against the rig: looks and feel

Join the rig (`~/q2playtest/run.sh`, then `connect 127.0.0.1:5073`). Change
mode with `~/q2playtest/cmd.sh "sv ruleset tdm"` then `cmd.sh "map q2dm1"`;
bots with `cmd.sh "bot_count 3"`. Check text rows in **both** fonts
(`scr_usekfont 0` and `1`).

| # | Try | Look for |
|---|---|---|
| 15 | Tab menu in every mode: join, team, vote, timeout pages | Readable, lines up, keyboard and controller both navigate it |
| 16 | Scoreboard in warmup, live and intermission (FFA, Duel, TDM, CTF) | Columns line up; ready marks, captain `*`, spectators, Duel's queue |
| 17 | HUD while live: the clock, the status line, a vote in progress, a timeout | Nothing overlaps the stock HUD, the weapon icon or chase-cam text |
| 18 | Frag messages and the place line | Short enough to read mid-fight; names show, never `##P<n>` |
| 19 | `callvote` from the menu and the console; `vote yes` / `yes` | Prompts clear, the result obvious |
| 20 | Spectate: chase cam, `eyecam`, `+use` | Smooth; first and third person both work |
| 21 | `bind c "cmd loc NEED WEAPON - %l"`, `bind e "cmd loc STATUS %h %a %w %l"` | The team-chat lines read well; names and places decode |
| 22 | `admin <password>` (set `admin_password` on the rig), then `admin` | The list is readable; replies come to you only |
| 23 | Duel `time-out`, then let it run out | The freeze and RESUMING countdown feel right |
| 24 | `cmd.sh "map mm-mcoil"` (all below height 0; `g_classic_jumps` is 1 by default): its jumps, stairs and ledges | **No prediction jitter**: this is the one movement change stock clients predict |
| 25 | Teleporters with `cmd.sh "set g_teleporter_freeze 0"`, then `1` (the default) | 0: out at speed, facing the exit, no stutter; 1: stock |

## Report

Results go in `docs/checklists/results/<date>-release.md` (the rows, pass or
fail, with notes), so this file stays a template. Tell Claude the commit
tested and anything that felt wrong; afterwards put the original
`game_x64.dll` back.
