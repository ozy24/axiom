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

Next in 2a:
- HUD stat-slot allocator; team service (needed by TDM/CTF in 3b).
- Relax `gamerules->integer && DMGame.X` gates as gametypes need hooks.
- M2a: FFA through the API (done in effect), CI green, single-player
  checklist again (human gate).

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
- None right now. The single-player checklist comes back at M2a.

## Local setup notes
- Format tools: `python3 -m venv ~/.local/share/q2game-tools &&
  ~/.local/share/q2game-tools/bin/pip install -r scripts/requirements-dev.txt`
  (`scripts/check_format.sh` finds it automatically).
