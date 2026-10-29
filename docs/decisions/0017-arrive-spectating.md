# 0017: New players arrive spectating with the menu; players stay in across map changes

**Decided:** 2026-10-29 (Chris)

Joining was stock: in FFA a new player spawned straight into the match, in
Duel straight into the queue (team modes already opened the join menu).
Now, as in MuffMode and MMREX (docs/design/joining.md):

1. **A human new to the server arrives spectating, with the Tab menu
   open**, and chooses to play: `join`, or the menu. The compiled default
   (Chris: "this should be the default"). `g_auto_join 1` puts everyone
   straight in; bots and a listen server's host always go in.
2. **Players stay in across map changes** (Chris). Under KEX every human
   reconnects at each map change; without this every map would start with
   everyone spectating. The last level's players come back as they were,
   on the same team, for the first 30 s. This also fixes TDM and CTF
   players losing their team at every map change.
3. **The memory is a cvar** (`g_join_memory`), as `map` reloads the module
   (the same reason `g_map_order` is one), with each social ID hashed to 8
   hex digits to keep it short.
4. **Duel's queue is opt-in** (Chris): only those who ask are queued, so a
   spectator is never pulled into a duel.
5. **`join` / `spectate` commands** in every gametype, which the menu runs.

**Reopen if:** players want a message of the day before the menu, or a
reconnect mid-level should also put a player back as they were (that is
the reconnect grace planned with timeouts).
