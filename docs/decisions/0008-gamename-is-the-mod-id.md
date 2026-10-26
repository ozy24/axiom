# 0008: serverinfo `gamename` is the mod id; `GAMEVERSION` stays "baseq2"

**Decided:** 2026-10-26

id registered serverinfo `gamename` from `GAMEVERSION` ("baseq2"). Neither
Q2RED nor Q2REX reads `gamename`, and the retail executable doesn't contain the
string: it only identifies the mod to server browsers and query tools, where
the convention is a short lowercase id. So `gamename` now comes from
`mod.json`'s `id` (`core/identity_register.cpp`), and `g_version` (serverinfo,
read-only) carries `<name> <version> (<build>)`.

`GAMEVERSION` itself is unchanged, because it is also the base game folder:
`g_svcmds.cpp` falls back to `GAMEVERSION/listip.cfg` when `game` is empty, and
that folder is `baseq2` whatever the mod is called.

The only edit to id's code is one line in `g_main.cpp`, marked `[mod]`, which
calls `Core_Identity_Register()` where the `gamename` line was.

**Reopen if:** an engine or browser is found to key behaviour off
`gamename == "baseq2"`.
