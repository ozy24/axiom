# 0010: Rogue's Tag and DeathBall stay stock-only, outside the registry

**Decided:** 2026-10-26 (revised the same day by Chris: "remove them from the
registry").

Tag (`gamerules 2`) and DeathBall (`gamerules 3`) are stock modes from the 1997
Ground Zero mission pack (`original/rogue/dm_tag.c`, `dm_ball.c`), still in the
rerelease source and the retail DLL, reachable only by the stock `gamerules`
cvar. They are not core gametypes, and the registry lists only Axiom's own
modes, so:

- `g_gametype` offers only the registry (today `ffa`).
- The stock `gamerules` cvar works exactly as in stock: when it is set,
  `Core_Gametype_Start` runs Rogue's `InitGameRules` untouched and
  `g_gametype` is ignored. The drop-in principle holds: an Axiom server can do
  everything a stock one can.
- The stock Tag crash fix (known id bug #1) stays, and harness tier 4 boots
  `gamerules 2` and `3` so it stays tested.

**Reopen if:** someone wants Tag or DeathBall as first-class modes; build them
as a downstream mod (or a registry gametype) then.
