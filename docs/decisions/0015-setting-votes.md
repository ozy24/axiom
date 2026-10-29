# 0015: Votes on a few settings, from a table compiled in

**Decided:** 2026-10-28 (Chris)

Players can vote on four settings as well as the map, the gametype and a
restart:

| Vote | Changes | Values | Where | Takes effect |
|---|---|---|---|---|
| `callvote timelimit <n>` | `timelimit` | 1-60 minutes | every gametype | at once, no restart |
| `callvote scorelimit <n>` (`fraglimit`, `capturelimit`) | `fraglimit`, or `capturelimit` in CTF | 0-100 frags, 0-20 captures; 0 = none | every gametype | at once, no restart |
| `callvote powerups on\|off` | stock's `g_no_items` | on / off | every gametype | reloads the map (items spawn as it loads) |
| `callvote friendlyfire on\|off` | stock's `g_friendly_fire` | on / off | team gametypes | at once |

1. **One table, compiled in** (`Core_Vote_Setting`, `core/vote.cpp`): each
   row names the setting, its kind of value, its range, the menu's choices,
   where it applies and whether it reloads. The checks, the messages, the
   HUD text and the Tab menu all come from it. Admins choose which votes are
   allowed (`g_vote_allow`), never what a vote may change: a "vote on any
   cvar" list is the road MuffMode's factories took (decision 0013), and
   lets players change what the admin never thought about.
2. **Fixed ranges**, wide enough for any sensible match and narrow enough
   that a vote can't make a silly one. The menu offers a few values (time
   5-30 minutes; frags 0-50; captures 0-10); the console takes any value in
   range. A vote for the value the setting has now is refused, and so is no
   score limit with no time limit (the match would never end).
3. **A voted value lasts until the gametype changes**, like an admin's
   `set`: map changes keep it, and the next ruleset switch resets it with
   every other gameplay cvar (decision 0013). `g_no_items` and
   `g_friendly_fire` joined `core_gameplay_cvars` so they do.
4. **On by default**: `g_vote_allow` lists all seven kinds.
5. **The limits change in place.** A match restart for "20 minutes, not 15"
   would throw the match away. The new time limit still counts from LIVE; a
   limit already passed ends the match, as stock's `CheckDMRules` would.
   Overtime keeps its own end. Powerups need a reload, which restarts the
   match from warmup like every other reloading vote.

Compared with MMREX (17 kinds, one hand-written handler pair each, a
disable bitmask): four settings from one table, an allow-list by name.

**Reopen if:** players want a setting the table lacks (add a row; the
table, not a new mechanism), or admins need ranges of their own.
