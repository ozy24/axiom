# 0002: id's files stay where they are

**Decided:** 2026-10-26

id's code mixes single player, coop and deathmatch inside the same functions
(`g_items.cpp`, `p_client.cpp`, `g_combat.cpp`; TDM lives in `ctf/g_ctf.cpp`).
There is no single-player file to move, so a folder split would mean splitting
those functions, which risks the save format and the drop-in guarantee.

Instead: mode checks become a few predicates and hook calls, edited in place in
small, tested steps. New code goes in new folders. Separation is enforced by the
gametype API, not by folders.

**Targeted splits** of one function are allowed later when it really gets in the
way, under the comparison and save tests, keeping every saved name. Each one
gets its own entry here.

**Reopen if:** never as a blanket move. Individual splits only, as above.
