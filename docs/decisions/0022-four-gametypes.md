# 0022: Four gametypes, no rounds in the base

**Decided:** 2026-10-30 (Chris).

Axiom is FFA, Duel, TDM and CTF, and only those. Clan Arena (0021) and
HoonyMode's rounds are removed; the last commit with them is tagged
`clan-arena-v1`.

**Why.** Every extra mode multiplies against every other system: rounds ×
votes × team locks × the scoreboard × joining × overtime. That is where
MuffMode's and MMREX's bugs come from, and where ours came from too: Clan
Arena added two match states, a round roster and about 15 hooks into id's
code, and most of the week's fixes were at those seams. Four modes keep the
grid small enough to test completely. Polish them before adding surface.

**What it means.**
- The match flow is warmup → countdown → live → overtime → intermission.
  No round states, no `match_rounds*`, no `match_suicide_point`.
- No built-in variants. An admin can still add one as a ruleset
  (`rulesets/<gametype>_<name>.cfg`, decision 0013); it changes settings,
  not code.
- Rounds, arenas, elimination and the like belong to downstream mods
  (Rocket Arena, Freeze Tag, a Clan Arena). If one needs a hook the base
  lacks, add the hook, not the mode. `clan-arena-v1` is a starting point.
- A new gametype in the base needs a new decision that supersedes this one.
