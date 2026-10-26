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
- Core gametypes: FFA, TDM, CTF, Duel. Variants (HoonyMode, Clan Arena) are
  rulesets, not code.
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
- **Tier 5 match-flow:** `match_warmup 1` with bots: the `Match:` lines must
  run warmup → countdown → live → intermission, ended by the timelimit counted
  from LIVE, on both engines (Q2REX's bots begin as clients even though they
  don't fight in a bot-only match).
- **Tier 6 duel:** three bots, `g_gametype duel`: one queues, the match runs to
  its timelimit, and a `Duel: … next: …` line shows the rotation.
- **Tier 7 rounds:** `match_rounds 1` with six bots: a frag must end a round
  and the next round start (Q2RED only; Q2REX's bots don't fight).
- **Tier 8 teams:** `tdm` on q2dm1 and `ctf` on q2ctf1 with four bots and the
  match flow: warmup → live → timelimit, and a match record with both teams
  and every playing client on one.

Each tier has been checked on Q2RED against a deliberately broken build (a soldier with
one extra health point, a renamed think function) and fails as it should.
The stock reference is the `stock-reference` tag (pinned in
`scripts/harness/pins.env`), never a fresh build of `main`.

Harness gotchas: coop save slots must be named `coop_*` or Q2RED reloads a
fresh map instead of the save; rig paths must stay under 900 characters
(Q2RED's `MAX_OSPATH`, 1024 since its A61); the engine binary is copied into each rig
so its crash logs land there. Locally, pass `--engine ~/q2ra2 --retail
"~/.local/share/Steam/steamapps/common/Quake 2/rerelease" --bots <dir>`.

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
   no movement changes. About 10 stat slots are free (`bg_local.h`). Ammo and
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
