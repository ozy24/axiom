# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 1c (first id edits, under tests)

M1a reached 2026-10-26: hosted CI green (checks, Linux, Windows/MSVC).

1b done:
- Jumpbox runner `jumpbox` (label `q2-game-tests`) online, running as the
  unprivileged `q2ci` user from `/srv/q2ci/runner`
  (`scripts/jumpbox/setup-ci-runner.sh`). Retail data in `/srv/q2ci/retail`.
- Harness `scripts/harness/q2harness.py`, tiers 1-3, all passing locally with
  stock as candidate; each tier proven to fail on a deliberately broken build.
- Calibration: 14 timing-volatile save fields, keyed by entity classname.
- `stock-reference` tag at `a44e2d63` (pinned in `scripts/harness/pins.env`).
- `.github/workflows/game-tests.yml` for the runner.

- Game tests green on the runner (Q2RED, tiers 1-3).
- Windows layer: Q2REX 0.15.1 under Wine runs the MSVC DLL, tiers 1-3 green
  locally; `windows-game-tests` job added to `ci.yml`. Stock DLL published as
  the `stock-reference` release asset (sha256 pinned).
- Calibration per engine: Q2RED and Q2REX both give the same 14 fields.

- **M1b reached** (pending merge of PR #2): every PR runs both builds, the
  static checks, and tiers 1-3 on Q2RED (`.so`) and on Q2REX under Wine (the
  MSVC `.dll`), all green on the runner.

1c, part 1 (identity) done:
- `mod.json` + `cmake/identity.cmake` -> generated `mod_identity.h`; build id
  from git. serverinfo `gamename` = id, `g_version`, banner, `mod-identity:`
  stamp. The one id edit: `g_main.cpp`, marked `[mod]`. `gamename` isn't in
  saves, so no allowed-differences entry; all tiers still pass.
- `scripts/check_name_leak.py` (in check.sh) and `scripts/rename_drill.sh`
  (CI job `rename-drill`). Decision 0008.

1c, part 2 done (PR "unit tests"):
- doctest unit tests (`game_tests`, run in both CI builds) over the `Host`
  seam (`core/host.h`, adapter `core/host_gi.cpp`, `tests/fake_host.h`);
  `scripts/check_core_seam.py`. Decision 0009.
- `docs/architecture.md`; `docs/checklists/single-player.md`.

**M1c needs one human gate:** Chris runs `docs/checklists/single-player.md`
on retail with the MSVC DLL from CI. Everything else for M1c is in place.

Next: Phase 2a (seams and the gametype API), which can start once M1c's
checklist passes.

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
- **Single-player checklist** (`docs/checklists/single-player.md`) for M1c.

## Local setup notes
- Format tools: `python3 -m venv ~/.local/share/q2game-tools &&
  ~/.local/share/q2game-tools/bin/pip install -r scripts/requirements-dev.txt`
  (`scripts/check_format.sh` finds it automatically).
