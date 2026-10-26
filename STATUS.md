# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 2a (seams and the gametype API)

M1c reached 2026-10-26: Chris ran the single-player checklist on retail with
the MSVC DLL from `8627ce73`; all eight steps passed.

2a, part 1 (gametype registry) done:
- `docs/design/gametype-api.md`: Rogue's `DMGame` is the one dispatch table;
  an Axiom registry (`core/gametype.h`, `gametypes/<id>/`, CMake
  `GAME_GAMETYPES`) selects by `g_gametype` (stock `gamerules` still works).
- ffa (stock DM, no hooks) is the only registry gametype. Rogue's Tag and
  DeathBall stay reachable only through stock `gamerules`, untouched (Chris,
  decision 0010). One `[mod]` line in `g_main.cpp` replaces the
  `InitGameRules` call.
- Known id bug #1 fixed: stock Tag crashed on every map (`droptofloor`).
- Harness tier 4: every registry gametype and stock gamerules 2/3 boot q2dm1
  with bots.

- HUD stat-slot allocator (`core/stat_slots.h`): slots 54..63, by owner,
  idempotent. The CTF block (18..31) is not handed out: `SetCTFStats` writes
  it every frame in every mode.
- **Deferred to 3b, deliberately:** the team service. Nothing uses it before
  TDM/CTF, so it gets designed against them (just in time, as the plan review
  advised). Call-site gates (`gamerules->integer && DMGame.X`) are relaxed
  when a gametype first installs a hook.

M2a status: the code side is done (FFA through the API, Rogue modes decided,
CI green). The single-player checklist for M2a is the remaining gate; it can
be run on any later build, so 2b continues meanwhile.

## Phase 2b, part 1 (match flow) done
- `core/match.*` state machine (24 unit-test cases) and adapter: warmup,
  ready/notready, countdown, live (timelimit from LIVE), overtime (sudden
  death or N min), intermission. Off by default: `match_warmup 0` is stock,
  so tiers 1-3 keep proving stock behaviour.
- Arena reset at LIVE: projectiles and dropped items removed, map items
  respawned, scores zeroed, players respawned. Doors/plats not reset yet.
- Harness tier 5. Bots keep fighting after the reset (7 kills in the minute
  after LIVE vs 10 in a stock minute, checked by hand).

Next, 2b part 2: rulesets as `.cfg` (built-in defaults, `ruleset <name>`),
then Phase 3a Duel.

## Known issues
- Q2RED crashes at startup if a dedicated server is started without `+map`
  (`q2ex_KexBotEntity_UnregisterEdict`, stock module too). The harness always
  passes `+map`. Not fixed yet; described in Q2RED's
  `game-error-unwind-after-unload-2026-10-26.md`.
- Q2REX's built-in bots never enter a bot-only match (retail admits bots
  only after a human spawns), so tier 3 can't check frags on Q2REX.
- `jumpbox.lan` returns a bogus AAAA record (`::`); use `ssh -4`.
- The Windows build has not run yet; first CI run will tell.
- The save-name baseline includes 49 names the compiler drops as unused, so
  saves can never contain them. Harmless; the check just errs on the safe side.

## Open human gates
- Single-player checklist for M2a, whenever convenient (any build from now on).

## Local setup notes
- Format tools: `python3 -m venv ~/.local/share/q2game-tools &&
  ~/.local/share/q2game-tools/bin/pip install -r scripts/requirements-dev.txt`
  (`scripts/check_format.sh` finds it automatically).
