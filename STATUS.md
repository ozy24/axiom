# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 1b (test runner and harness)

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

Blocked on Chris:
- `sudo chmod 0751 /srv/q2ci` on the jumpbox (the setup script's first run
  made it 0750, so chris can't reach `engines/`; the script is fixed). Then
  run `bash ~/q2ci-scripts/jumpbox/install-engines.sh` there, or ask Claude to.
  Until then the game-tests workflow fails at the harness step.

Next in 1b:
- First green game-tests run on the runner; Windows DLL under Q2REX/Wine
  (Wine is not installed on the jumpbox yet).

## Known issues
- Q2RED segfaults inside its own error handler after a fatal game error
  (seen when tier 2 loads a save with an unknown function name). The harness
  reports it correctly; it's a Q2RED bug, not ours.
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
