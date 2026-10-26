# 0007: Stock saves must always load in Axiom; the reverse only temporarily

**Decided:** 2026-10-26

With `g_strict_saves 1` (the default, `g_main.cpp:310`), loading a save with an
unknown field (`g_save.cpp:2350`) or unknown function name (`g_save.cpp:1759`)
is fatal. So:

- **Stock → Axiom must always work.** Never rename or remove id's saved names.
  Enforced by `scripts/check_save_baseline.py`.
- **Axiom → stock** works only while Axiom saves no state of its own. It is
  required during the Phase 2a refactors, which must not change the save
  format. After that it is allowed to break.
- Gametype per-player data is not saved in coop.

**Reopen if:** Axiom ever needs players to move saves back to a stock install.
