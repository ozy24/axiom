# 0011: Use the engine's file access when it offers it, never require it

**Decided:** 2026-10-26

Q2RED's `GetExtension("Q2RED_FILESYSTEM_API_V1")` (its ledger A62) gives a game
module `LoadFile` through the search path and an atomic `WriteFile` under the
game directory, for game paths only. Retail stores null in `gi.GetExtension`
and Q2REX doesn't offer it, so:

- `core/host_files_gi.cpp` asks once per load, checking the slot isn't null
  first (calling retail's would crash). `core/host_files_ext.h` is a frozen
  copy of the v1 layout; a new version gets a new name, never an edit.
- Written output uses it when present: a relative `g_matchlog_dir` (e.g.
  `matches`) goes under the game directory, beside the saves. Without it, or
  for an absolute directory, the mod writes the path itself, relative to the
  server's working directory, as before.
- Rulesets stay `.cfg` files run with `exec` (decision 0003): `exec` already
  reads paks on every engine, so `LoadFile` has no user yet.

Harness tier 6 checks the record lands in `baseq2/matches` on Q2RED and in the
working directory on Q2REX.

**Reopen if:** Q2REX or retail gains file access (then 0003's reopen condition
may be met), or a feature needs to read data that `exec` can't carry.
