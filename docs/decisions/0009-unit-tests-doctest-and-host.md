# 0009: Unit tests use doctest and a Host seam

**Decided:** 2026-10-26

The game tests prove whole games but take minutes and say little about which
line broke. The mod's own logic is written as plain functions over a small
`Host` interface (`core/host.h`) and tested with a fake in milliseconds.

- **doctest 2.4.11**, vendored: one MIT header, widely used, standard
  assertions, no build dependency. Tests only; never in the module.
- **Host, not a fake `game_import_t`:** faking `gi` would pull `g_local.h`
  and id's globals into every test. Host starts with `Print` and `TimeMs` and
  grows only when a module needs more.
- **Adapters** (`*_register.cpp`, `*_gi.cpp`) are the only `core/` files that
  include `g_local.h`; `scripts/check_core_seam.py` enforces it.

**Reopen if:** tests need engine behaviour Host can't reasonably model. Then
add a harness test instead.
