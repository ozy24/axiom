# 0025: Match timeouts

**Decided:** 2026-11-01 (Chris: "i think just one timeout per match, per
side makes sense. doesn't need the anti-escape rule").

A player's connection drops, or a team needs a sub mid-match, and the
match plays on without them. MMREX has timeouts (`timeout` / `timein`,
`g_dm_timeout_length`, `g_dm_timeout_resume_countdown`); we took its core
and left its reconnect system ("auto ghost", about 3,000 lines) and its
per-arena allowances.

**Decided:**
- **Where:** any match the match flow runs, while it's live or in
  overtime; never in warmup, the countdown or the intermission. No match
  flow, no timeouts.
- **Who:** players in the match. **One per side per match**: a side is a
  team in TDM and CTF, and a player (by social ID, so a reconnect doesn't
  give another) in Duel and FFA. MMREX gives one per player, so a 4v4 had
  eight. A new match (LIVE) gives every side its timeout back. The server
  console (`sv timeout`, `sv timein`) needs no allowance.
- **Ending one:** it runs `match_timeout` seconds, then a resume
  countdown of `match_timeout_resume` seconds (spoken, the big digits),
  then FIGHT!. The calling side may end it early with `time-in`, which
  starts the countdown; the other side may not. The server ends any.
- **The world stands still:** the game frame stops before level time
  moves (MMREX's way), so every timer, item, projectile, powerup and the
  match clock pause with it, with no pause code of their own. Players get
  `PM_FREEZE` (no moving, firing or `kill`); an open Tab menu still works.
  No vote can be called during one, and an open one waits it out.
- **Commands:** `time-out` / `time-in`, as MMREX. Retail's engine has a
  `timeout` cvar (the client timeout), so a player who types `timeout`
  sees that cvar and nothing reaches the server; `timeout` / `timein` are
  still accepted where they do arrive. The Tab menu has Call Timeout / End
  Timeout.
- **Settings:** `match_timeout` (seconds; compiled default `0`, off, as
  stock has none), `match_timeout_resume` (`10`). The duel, tdm and ctf
  rulesets set 120; ffa leaves it off: in a public FFA one player
  shouldn't freeze everyone.
- **No anti-escape rule** (a short no-call window after taking damage):
  with one timeout a side, using it to break an opponent's momentum is
  a tactic it costs them. Chris, 2026-11-01.

core/timeout.cpp is the logic (tests/test_timeout.cpp); the adapter is
core/timeout_register.cpp, with two marked hooks in id's code (G_RunFrame_,
ClientThink). Harness tier 25 checks it with simulated players.

**Reopen if:** timeouts are used to stall in practice (an anti-escape rule,
or fewer); a team game wants more than one a side; or the reconnect case
needs an automatic timeout on a drop (MMREX's `g_auto_ghost_timeout`).
