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

- **M1a reached.** Private remote `ozy24/axiom`; hosted CI green on the first
  run: checks, Linux (Ubuntu 22.04, GCC 12, static runtime, exports only
  `GetGameAPI`) and Windows (MSVC, exports `GetGameAPI` and `GetCGameAPI`).

Next: Phase 1b, the isolated jumpbox runner and the Q2RED harness.

Blocked on:
- **The jumpbox is unreachable** (2026-10-26 ~13:00: no ping reply from
  192.168.16.191). Also, DNS returns a bogus AAAA record `::` for
  `jumpbox.lan`, so plain `ssh jumpbox.lan` connects to OZY1's own sshd and
  fails host-key verification. That is not an attack; use `ssh -4` or fix the
  DNS record. Never accept the changed key.

## Known issues
- MSVC prints conversion warnings (C4244/C4267) in id's code at /W3. Expected;
  id's code is not ours to clean up.
- The save-name baseline includes 49 names the compiler drops as unused, so
  saves can never contain them. Harmless; the check just errs on the safe side.

## Open human gates
- Merge policy (Claude merges on green CI, or Chris merges). Until decided,
  Claude opens PRs and Chris merges.

## Local setup notes
- Format tools: `python3 -m venv ~/.local/share/axiom-tools &&
  ~/.local/share/axiom-tools/bin/pip install -r scripts/requirements-dev.txt`
  (`scripts/check_format.sh` finds it automatically).
