# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 1a (repo and build, no id edits)

Done:
- Pristine id import at `8dc1fc9` (first commit).
- CMake build: Windows (MSVC, static CRT) and Linux (GCC, hidden visibility,
  export map). Vendored fmt 10.2.1 and jsoncpp 1.9.6.
- Linux module checked in Q2RED by hand: loads, runs InitGame, spawns q2dm1.
- Ubuntu 22.04 / GCC 12 release build checked in a container: depends only on
  libc and libm (glibc 2.35), exports only `GetGameAPI`.
- Static checks: save-name baseline (1,652 names), stock-client contract
  `static_assert`s, repo hygiene, clang-format 19.1.7 (pinned via pip).
- `AGENTS.md`, decision log 0001–0007.
- Day-1 question answered: the retail DLL matches the source's save surface
  (decision 0001).

In progress:
- Hosted CI (`.github/workflows/ci.yml`) on the private GitHub remote.

Next:
- M1a: hosted CI green on Windows and Linux.
- Then Phase 1b: the isolated jumpbox runner and the Q2RED harness.

## Known issues
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
