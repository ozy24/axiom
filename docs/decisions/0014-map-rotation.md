# 0014: One map list per gametype; stock's rotation, shuffled in passes

**Decided:** 2026-10-28 (Chris)

Each gametype's config sets one list, `g_map_list`: the maps that rotate
and the maps a vote offers. There is no separate vote-only pool
(MuffMode's and MMREX's `g_map_pool`), no structured pool or cycle file
(MMREX's `g_maps_pool_file` / `g_maps_cycle_file`), no per-map player
limits, no MyMap queue and no end-of-match map pick. Ease of administration
comes first (decision 0013): one list to keep, in the file the admin
already edits.

Stock's `EndDMLevel` already walks `g_map_list`. Axiom keeps it and changes
three things, in `core/maps.*` behind one marked call:

1. **A map off the list is followed by the list's first.** The server's
   starting map (`+map`, a listen server's menu) or an admin's `map` need
   not be listed; stock then follows the map's own exit and may never
   reach the list.
2. **Shuffled from the first pass** (`g_map_list_shuffle 1`, set by every
   built-in ruleset), starting from the current map if it's listed, so the
   whole pass plays. Stock plays the first pass in written order.
3. **A shuffle bag with a gap.** Every map plays once per pass; each new
   pass is random except that no map comes back until at least half the
   list (rounded down) has played since its last turn. A plain reshuffle
   could put the last map of one pass second in the next; a fixed order
   would be predictable. Two maps alternate; three never repeat back to back.

The play order lives in `g_map_order` (a cvar survives the module reload
each `map` causes); the admin's `g_map_list` is never rewritten, as stock's
shuffle does. The reshuffle is seeded from the pass just played, so the
Tab menu's "Next map" and the level end agree. With no list, or under
Rogue's stock gamerules modes (decision 0010), stock's code runs unchanged.

**Considered and left out:**
- A vote-only pool. It adds a second list, and a voted pool map would
  need the rotation to remember where it was. Add it (with that) if an
  admin asks.
- Resetting an empty server to its starting mode and map. Chris decided
  against it (2026-10-28): a server stays where its players left it.
- MMREX's repeat delay in seconds. The gap rule above needs no tuning.

**Reopen if** admins want maps that can be voted but don't rotate, or the
rotation needs to depend on the player count.
