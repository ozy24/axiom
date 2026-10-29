# 0019: Duel's own scoreboard

**Decided:** 2026-10-29 (Chris)

Duel showed stock's FFA board: the two players' blocks and nothing about
the queue. Chris likes MMREX's duel board; we follow it, with changes
(docs/design/scoreboard.md):

1. **MMREX's shape:** a "Duel on <map>" title, the two duellists in
   stock's blocks with ready ticks, the queue and the spectators below.
2. **Better than MMREX's:** the queue is numbered (in winner-stays-on the
   order is what people read); each duellist's record (wins, losses,
   streak) shows under their block, not only the queue's; spectators show
   their record instead of MMREX's "0 0"; the "1st place with a score of
   N" line is gone; at intermission the match time and WINNER over the
   winner's block.
3. **A gametype hook,** `GametypeDef::scoreboard`, under the same
   `g_scoreboard` as FFA's board (decision 0018). The built-in `duel`
   ruleset sets 1.
4. **Records last for the session in memory:** across `gamemap`, not a
   `map` reload. A forfeit isn't counted.

**Reopen if:** records should survive a `map` reload (a cvar, as
`g_join_memory`), or forfeits should count.
