# 0021: Clan Arena: its own data-only gametype on elimination rounds with lives

**Decided:** 2026-10-29/30 (Chris), after comparing MMREX's and MuffMode's CA.

1. **Its own gametype id `ca`**, with no code of its own (`gametypes/ca` is
   data on stock teamplay). The Q2REX browser only labels a server "Clan
   Arena" for mode id `ca`, a TDM variant would nest under TDM in the vote
   menu, and an admin's `rulesets/tdm.cfg` would leak into CA. The principle
   "variants are rulesets, not code" holds: CA's rules are match-flow
   settings any ruleset can use.
2. **Elimination rounds with lives** (`match_rounds elimination`,
   `match_round_lives`), not a CA-only rule: Clan Arena is one life, Quake
   Champions' Duel is three. `match_rounds` became `0|frag|elimination`
   (`1` still means frag).
3. **The dead follow teammates only, no free camera** (competitive).
4. **Round countdown: move but no fire** in CA; `match_round_freeze` gives
   QC's frozen start, and then round 1 gets its own countdown.
5. **Score: frags + damage/100** by default (`match_damage_score`).
6. **Defaults: 150 health / 150 armor, first to 8, 2-minute rounds**, sudden
   death on a tie, team lock at LIVE.
7. **MMREX's bugs fixed** (docs/design/clan-arena.md): mutual wipe is an
   immediate draw; a leaver never counts as alive; the damage score keeps its
   remainder and counts only in play; the limit is "first to N"; ammo never
   over its cap; the timelimit waits for the round; an empty team gets no
   free rounds.
8. `g_dm_no_self_damage` reuses MuffMode's name: it behaves the same
   (decision 0004).

**Reopen if:** CA needs behaviour a match-flow setting can't express (then it
gets hooks like Duel's), or QC Duel needs a different round rule.
