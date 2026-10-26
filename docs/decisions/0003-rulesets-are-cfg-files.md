# 0003: Rulesets are .cfg files run through exec

**Decided:** 2026-10-26

`game_import_t` has no filesystem API. A `.cfg` run with `exec` goes through the
engine's own filesystem (paks and the user folder) on every engine, with no
parser in the mod. The DLL also carries built-in defaults, so it works with no
files at all.

**Reopen if:** an engine extension gives the game module real file access on all
supported engines.
