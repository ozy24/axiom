# Working on this repo

Read this first, then `STATUS.md` (where things stand) and `docs/decisions/`
(what is already settled). The full phased plan lives outside the repo at
`~/.claude/plans/crispy-brewing-hartmanis.md` on Chris's desktop; `STATUS.md`
carries the parts that matter for the next step.

This mod is maintained mainly by AI sessions that share no memory. So: **every
rule that matters is enforced by a check, not remembered.** If you find a rule
that is only written down, add the check.

## What this is

A Quake II Rerelease game module (`game_x64.dll` / `game_x64.so`) built on id
Software's GPL `quake2-rerelease-dll`. The working name is Axiom; the name lives
in one place, `mod.json`, and must never be hard-coded (see "Identity" below).

Principles:
- Core gametypes: FFA, TDM, CTF, Duel, and only those (decision 0022):
  polish them before adding surface. Every extra mode multiplies against
  every system. A variant is an admin's ruleset, not code; rounds, arenas
  and the like belong to downstream mods.
- Streamlined: no over-complicated systems such as voting factories. Every
  feature earns its place.
- A base for downstream mods (Jump, Rocket Arena, Freeze Tag, Horde) to build on
  without forking.
- **A safe drop-in:** players install by overwriting `baseq2/game_x64.dll`, so
  single player, coop and the mission packs must keep working exactly as stock.
- Stock-client compatible; faithful to stock gameplay by default; the server
  never trusts the client.
- Developed on Linux, played mostly on Windows: both builds are run, not just
  compiled.

## Layout

| Path | What |
|---|---|
| `rerelease/` | id's game source. **Edited in place, never moved** (see `docs/decisions/0002`). |
| `platform/posix/` | Linux entry-point shim, compat header, export map |
| `third_party/` | Vendored fmt 10.2.1 and jsoncpp 1.9.6 (`docs/decisions/0006`) |
| `checks/` | Compile-time checks built into the module |
| `scripts/` | Checks and tooling. Each script says what it enforces. |
| `tools/` | One-off generators (each says how and when to run it) |
| `docs/baselines/` | Generated baselines the checks compare against |
| `docs/decisions/` | One file per settled question. Add one when you settle something. |
| `repos/` | Local reference checkouts (Q2REX, MuffMode, id). **Git-ignored; never commit.** |

New code (from Phase 2) goes in small, focused files under `core/` and
`gametypes/<name>/`, never into id's large files beyond hook calls.

## Identity

- `mod.json` holds `name`, `id`, `version` and `repository`. `cmake/identity.cmake`
  reads it (plus the git commit as the build id, `nogit` from a source archive)
  and generates `mod_identity.h` (`MOD_NAME`, `MOD_ID`, `MOD_VERSION`,
  `MOD_BUILD`). Downstream mods pass `-DMOD_IDENTITY_FILE=<their mod.json>`.
- At run time `core/identity*.cpp` sets serverinfo `gamename` = id and
  `g_version` = `<name> <version> (<build>)`, prints the banner, and keeps a
  greppable `mod-identity:` stamp in the binary (`docs/decisions/0008`).
- `scripts/check_name_leak.py` fails if the name or id appears anywhere but
  `mod.json`, docs (`*.md`, `docs/`) and the GitHub repo slug.
  `scripts/rename_drill.sh` (CI job `rename-drill`) rebuilds under a dummy name
  and checks the binary. Don't bypass either: get the name from the header.

## Gametypes

`docs/design/gametype-api.md` is the reference. In short: Rogue's
`dm_game_rt DMGame` is the one dispatch table; `core/gametype.h` is a registry
of `GametypeDef`s (one per `gametypes/<id>/gametype.cpp`, listed in CMake's
`GAME_GAMETYPES`); `g_gametype` selects one. The stock `gamerules` cvar still
runs Rogue's Tag/DeathBall exactly as stock; they are not in the registry
(decision 0010). Add hooks only when a gametype needs them, as marked one-line calls.

## Build

```bash
cmake --preset linux-release && cmake --build --preset linux-release
# output: build/linux-release/game_x64.so
```

Windows release builds come only from MSVC on GitHub's Windows runner
(`windows-msvc-release` preset: Ninja driving MSVC's `cl.exe`, so CI can cache
compiles with sccache). A locally cross-compiled DLL is never shipped.

## Checks

```bash
scripts/check.sh          # everything hosted CI runs that needs no game data
```

Hosted CI (`.github/workflows/ci.yml`) runs the same script plus both builds,
the unit tests and the rename drill.

**Unit tests** (`tests/`, doctest, target `game_tests`) cover the mod's own
logic without the engine:

```bash
ctest --test-dir build/linux-release --output-on-failure
```

Logic takes a `Host&` (`core/host.h`) and never includes `g_local.h`; tests pass
`tests/fake_host.h`. Only adapters (`*_register.cpp`, `*_gi.cpp`) touch gi or
level, and they stay out of `game_tests`. `scripts/check_core_seam.py`
enforces this. New logic comes with tests. See `docs/architecture.md`.

Game tests run on the self-hosted jumpbox runner, because they need retail
data that exists only there. The same tiers run on two engines:

- **Q2RED + `game_x64.so`** (`.github/workflows/game-tests.yml`), the fast native
  layer, against a stock `.so` built from the `stock-reference` tag.
- **Q2REX under Wine + the MSVC `game_x64.dll`** (the `windows-game-tests` job
  in `ci.yml`), so the binary players actually run is executed on every PR.
  The stock DLL is the `stock-reference` release asset, hash-pinned in
  `scripts/harness/pins.env`. Q2REX runs in an offline network namespace
  (`unshare -rn`), because it signs in to PlayFab at startup even on LAN.



```bash
python3 -I scripts/harness/q2harness.py test --stock STOCK.so --candidate build/linux-release/game_x64.so
```

CI runs the Q2RED tiers four at a time (`--jobs 4`, each on its own ports:
about 5 minutes); Q2REX stays one at a time (a shared Wine prefix). A change
to docs alone (`*.md`, `docs/`) skips both game-test jobs.

- **Tier 1 save-diff:** coop base1 saved by stock and by the candidate; the
  save JSON must match except fields in `docs/baselines/volatile-save-fields.<engine>.txt`
  (timing noise, measured per engine by `q2harness.py calibrate`) and intentional changes
  in `docs/baselines/allowed-differences.txt` (each linked to
  `docs/known-id-bugs.md`).
- **Tier 2 load-stock:** a stock save must load in the candidate, proven by the
  engine's `SV_ReadLevelFile()` line.
- **Tier 3 dm-smoke:** six bots on q2dm1 for 60 s; no errors, clean
  shutdown, and frags on Q2RED (ozbot). Q2REX's built-in bots never enter a
  bot-only match, so there it only checks that deathmatch boots with bots
  connected.
- **Tier 4 gametype-boot:** every registered gametype (`--gametypes`, default
  `ffa,duel,tdm,ctf`; keep it in step with `GAME_GAMETYPES`) and Rogue's stock
  `gamerules` 2 and 3 (`--stock-gamerules`) boot q2dm1 (ctf: q2ctf1) with bots
  for 20 s, check the Q2REX lobby cvars, log their
  `Gametype:` line and shut down cleanly.
- **Tier 5 match-flow:** `match_flow 1` with bots: the `Match:` lines must
  run warmup → countdown → live → intermission, ended by the timelimit counted
  from LIVE, on both engines (Q2REX's bots begin as clients even though they
  don't fight in a bot-only match); going live puts the movers back
  (`Movers: N reset`, q2dm1's two plats).
- **Tier 6 duel:** three bots, `g_gametype duel`: one queues, the match runs to
  its timelimit, and a `Duel: … next: …` line shows the rotation.
- **Tier 9 warmup:** six bots fight in a warmup that never starts
  (`match_minplayers 99`); `sv match` must show every score still 0, and
  the warmup loadout names q2dm1's weapons (Q2RED only).
- **Tier 10 duel-rotation:** after a Duel ends the level changes
  (`gamemap`); the loser must wait and the queued bot play (Q2RED only).
- **Tier 11 human-duel:** three simulated players (Q2RED `q2red_sim_client`,
  played by ozbot as puppets, `docs/design/simulated-clients.md`) join like
  retail clients and run the playtest's match-flow and Duel rows; checks read
  the server's game tap (what each player was shown). Duel's scoreboard:
  the queue (place and ping) in warmup, the winner at
  intermission. Q2RED only; skipped when the engine has no sim client.
- **Tier 12 human-teams:** `tdm` and `ctf` with four simulated players who
  pick teams with `team red` / `team blue`: no countdown while one team is
  empty, then a timelimit end and a record with everyone on their team.
- **Tier 14 human-vote:** three simulated players in an FFA warmup: a
  refused map vote fails, its caller waits out the cooldown, a map off the
  list is answered with the list, then a vote answered with stock's `yes`
  passes, shows on the HUD's status line and loads the map. Carol can `use`
  the warmup loadout's railgun, but not the BFG q2dm1 doesn't place.
- **Tier 15 maps:** a shuffled three-map `g_map_list` (q2dm3-5), started off the list
  on q2dm1, with bots: four level ends, each followed by a `gamemap` to the
  announced map; the first pass plays every listed map once, the next pass
  doesn't open with the map just played, `g_map_list` is unchanged,
  and the console can't set the internal `g_map_order` (decision 0004).
- **Tier 16 human-settings:** three simulated players in a live FFA match:
  a time limit vote moves the clock without a restart, friendly fire and
  an out-of-range time are refused, and a powerups vote reloads the map
  with the voted time limit kept. The column scoreboard (decision 0018)
  shows a suicide as a death; a frag between them tells both players
  (`g_frag_messages`), the place line while live.
- **Tier 17 human-teamlock:** TDM with five simulated players on the
  built-in ruleset (`match_lock 1`): first-on captains, both teams locked
  at LIVE, a newcomer refused (told alone), a dropped player back in, the
  captain's unlock / relock / `teamkick`, the captaincy passed on when the
  captain leaves, an emptied team unlocked, and each captain's `*` on
  the scoreboard.
- **Tier 18 human-join:** the default arrival (`g_auto_join 0`) in FFA,
  Duel and TDM: a newcomer spectates with the menu open and joins by `join`
  or the menu; after `gamemap` and `map` (a module reload) the last level's
  players are back as they were (TDM: on their team) and a watcher still
  watches; Duel queues only those who asked. Tiers 11, 14 and 16 set
  `g_auto_join 1`: they test what comes after joining.
- **Tier 19 human-readyvote:** TDM in a ready-up warmup with four
  simulated players: `callvote readyteam` is voted on by the caller's
  team alone (the other team is refused) and readies it; `callvote
  readyall` readies everyone and the match goes live.
- **Tier 20 human-shuffle:** TDM warmup with five simulated players, RED
  locked by the server: a passed `callvote shuffle` gives a 3/2 split that
  isn't the old groups, every team skin follows, RED is unlocked; once
  live, a shuffle is refused (Q2RED only).
- **Tier 22 entities:** a q2dm1 `.ent` override (the BSP's entities plus
  MuffMode's keys, small ammo, a jump pad and its targets) in FFA and
  Duel: the file replaces the BSP's, each mode leaves out what its keys
  say, nothing is unknown, the pad is aimed, a map-sized trigger fires
  `target_remove_powerups` at the bots, and `sv saveents` writes
  q2dm2's list but never over an existing file (docs/design/entities.md).
- **Tier 23 startup-ruleset:** an empty server started with `set g_ruleset
  ffa`: rulesets/ffa.cfg's settings and a file it execs are in force as the
  first map spawns (its `.ent` folder is the file's), with nobody connecting.
- **Tier 24 admin-commands:** the console's match commands. TDM with four
  simulated players: `sv readyteam red` readies RED alone, `sv readyall`
  starts the match, `sv lockteam` locks both, `sv spectate` takes a player
  off RED and the lock keeps him out. Duel: `sv spectate` on a duellist
  while live goes back to warmup (no forfeit) and the queued player steps
  up (Q2RED only).
- **Tier 25 human-timeout:** Duel with two simulated players in a live
  match: Alice's `time-out` stops the world (`sv match` four seconds apart:
  the same scores, nobody moved; the clock stands still, the status line
  counts down to RESUMING); Bob's own call, his `time-in`, `kill` and
  `callvote` are refused; Alice's second call is refused (one a match); Bob
  calls his and ends it early with `time-in` (Q2RED only).
- **Tier 8 teams:** `tdm` on q2dm1 and `ctf` on q2ctf1 with four bots and the
  match flow: warmup → live → timelimit, and a match record with both teams
  and every playing client on one.

Each tier has been checked on Q2RED against a deliberately broken build (a soldier with
one extra health point, a renamed think function) and fails as it should.
The stock reference is the `stock-reference` tag (pinned in
`scripts/harness/pins.env`), never a fresh build of `main`.

Harness gotchas: scenario settings go in an exec'd cfg (too many `+set`s pass
the engine's argument limit); coop save slots must be named `coop_*` or Q2RED reloads a
fresh map instead of the save; rig paths must stay under 900 characters
(Q2RED's `MAX_OSPATH`, 1024 since its A61); the engine binary is copied into each rig
so its crash logs land there. Locally, `scripts/harness/setup-local-kit.sh`
builds the pinned engines, bots, retail links and stock reference into
`~/.cache/q2harness-kit` once, and the harness then needs only `--candidate`.
**Never keep reusable kits in `/tmp`**: a reboot clears it.

## Hard rules

These come from id's code and the engine; each was verified in source.

1. **Saves.** Saved keys are struct member names (`FIELD`/`FIELD_AUTO` in
   `g_save.cpp`) and registered function/MMOVE names (the macros in
   `g_local.h`). With `g_strict_saves 1` (the default) an unknown one is fatal.
   **Never rename or remove a saved field or registered name in id's code.**
   `scripts/check_save_baseline.py` enforces this against
   `docs/baselines/save-names.txt`. Adding names is fine; regenerate the
   baseline with `--update` and commit it with the change.
2. **Stock clients run their own cgame.** Movement is predicted client-side, so
   no movement changes (one exception, Chris's: original Quake II's jumps
   below height 0, `g_classic_jumps`, decision 0024). About 10 stat slots are free (`bg_local.h`). Ammo and
   powerup stats are bit-packed by enum index, so no new ammo or powerup types.
   New `$loc` keys show raw on stock clients, so ship no loc file. The
   `static_assert`s in `checks/stock_client_contract.cpp` pin every stat,
   ammo and powerup index to its stock value; never regenerate them to make a
   failure go away.
3. **No filesystem API** in `game_import_t`. Rulesets are `.cfg` files run with
   `exec`; written output goes to a cvar-configured path. Q2RED offers optional
   file access through `GetExtension` (`core/host_files*`, decision 0011): use
   it when present, never require it, and check `gi.GetExtension` isn't null
   (retail leaves it null).
4. **Public names are neutral.** Cvars and commands use `g_*`, `match_*`,
   `sv_*`, never the mod's name or a mod prefix (`docs/decisions/0004`).
5. **Portability.** Use fixed-width integer types where size matters; never
   assume file-name case; both platforms build on every PR.
6. **Chat is not a command channel.** Under `KEX_Q2_GAME`, `say` goes through
   the lobby; commands are console commands.
7. **Never commit `repos/`.** `scripts/check_repo_hygiene.sh` enforces it.

## Workflow

- One branch and PR per change. CI must be green.
- **Playtest feedback is batched** (Chris, 2026-10-27): while Chris is testing
  on the playtest rig, fixes go onto one branch and onto the rig for him to
  retest, with only the relevant tiers run locally. The PR - and the full CI
  run - waits until he says he's done.
- Every change to id's code is small and reviewable on its own, and every
  edited line in `rerelease/` carries a `// [mod]` comment, so
  `git grep -n "\[mod\]" rerelease/` lists every place we differ from stock.
  Prefer a one-line call into new code under `core/` over logic inside id's files.
- Every bug fix comes with a test that reproduces it (once the harness exists).
- **Merge policy (Chris, 2026-10-26):** Claude merges its own PRs once every
  check is green, hosted CI and the jumpbox game tests alike. Squash-merge.
- End every session by updating `STATUS.md`.

## Glossary

- **Q2RED:** native Linux reconstruction of the rerelease dedicated server
  (`quake2ex_dedicated`, `~/Code/Projects/q2red`). Loads `game_x64.so`.
- **Q2REX:** Windows sourceport and dedicated server; runs under Wine on Linux.
  Loads `game_x64.dll`.
- **ozbot:** bot brain for Q2RED (`~/Code/Projects/ozbot-re`), loaded with
  `sv_bot_brain`. Needs per-map nav files.
- **Jumpbox:** `jumpbox.lan`, the headless box that will host the self-hosted CI
  runner.
- **Stock reference:** this repo's build of unmodified id code at `8dc1fc9`, the
  baseline Axiom is compared against.
