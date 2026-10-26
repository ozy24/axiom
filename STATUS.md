# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 1b done, 1c next

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

Next: Phase 1c (first id edits, under tests):
- `mod.json` identity and generated header; change `GAMEVERSION`/`gamename`
  and split the `listip.cfg` path; `gamename` onto the allowed-differences
  list if it shows up in saves.
- Name-leak check, version from git, rename drill.
- Unit-test framework with the `Host` seam.
- Manual single-player checklist written for Chris (human gate).

## Known issues
- Q2RED segfaults inside its own error handler after a fatal game error
  (seen when tier 2 loads a save with an unknown function name). The harness
  reports it correctly; it's a Q2RED bug, not ours.
- Q2REX's built-in bots never enter a bot-only match (retail admits bots
  only after a human spawns), so tier 3 can't check frags on Q2REX.
- `jumpbox.lan` returns a bogus AAAA record (`::`); use `ssh -4`.
- The Windows build has not run yet; first CI run will tell.
- The save-name baseline includes 49 names the compiler drops as unused, so
  saves can never contain them. Harmless; the check just errs on the safe side.

## Open human gates
- Merge policy (Claude merges on green CI, or Chris merges). Until decided,
  Claude opens PRs and Chris merges.

## Local setup notes
- Format tools: `python3 -m venv ~/.local/share/axiom-tools &&
  ~/.local/share/axiom-tools/bin/pip install -r scripts/requirements-dev.txt`
  (`scripts/check_format.sh` finds it automatically).
