# 0001: The retail game DLL matches id's source closely enough for a drop-in

**Decided:** 2026-10-26

## Question
The drop-in principle assumes a build of id's published source (`8dc1fc9`,
2023-10-03) can replace the retail `baseq2/game_x64.dll` without breaking single
player. Retail could have been updated since.

## Finding
The stock retail DLL (`game_x64.dll.orig` in the Steam install, sha256
`045d49c53722d9b9…`) has a PE build timestamp of **2025-10-23**, two years after
the source. But its save surface matches the source:

- Every saved field name (`FIELD`/`FIELD_AUTO` in `g_save.cpp`) appears in the
  retail binary.
- All 1,115 registered save function and MMOVE names appear, except 29 that the
  source defines but never references (for example `brain_move_defense`), which
  the retail linker dropped as dead code.
- No identifier in the retail binary is missing from the source.

So retail is a rebuild of essentially this source. Saves are compatible both
ways, and a drop-in build does not change the save format.

## Limits
This compares names, not function bodies. Retail may contain behaviour fixes the
source lacks. The comparison tests (Phase 1b) run against this repo's build of
`8dc1fc9`, not against retail, so they cannot detect that. The manual
single-player checklist on retail is the guard.

## Reopen if
A retail update changes the save surface (re-run the comparison in this file
against the new DLL), or players report single-player differences between
retail and an Axiom drop-in.
