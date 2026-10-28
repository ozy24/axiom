# The Tab menu

One menu for everything a player does in deathmatch: join, spectate, chase,
ready, vote, read the match settings. It replaces stock's inventory (Tab) and
CTF join menu under a registered gametype. Single player, coop and Rogue's
stock `gamerules` modes keep stock's.

## How it's built

- `core/menu.*` (pure, unit-tested) builds each page from a `MenuView`
  snapshot: rows of text, their action and an argument.
- `core/menu_register.cpp` fills the view from the level and turns a page into
  stock's pmenu entries. Stock's code then does the drawing, the cursor
  (invnext / invprev, forward / back for a free spectator), selecting
  (invuse, fire, jump), closing, and the 3-second refresh.
- Every action runs an existing command's code (`team`, `spectator`,
  `ready`, `callvote`, `vote`...), so the menu adds no game logic, and the
  console and the menu always agree.

## Limits

- 18 rows fit under the inventory picture (yv 32 + 8 per row).
- A left row starts at xv 64, so 28 characters fit inside the 320 box.
  Longer text is clipped with "...".
- The whole layout must fit the client's 1024-byte buffer.
  `Core_Menu_Layout` reproduces stock's `PMenu_Do_Update` byte for byte, and
  tests hold the longest page to the limit. That is why a choice list holds
  14 rows: 15 came to 1029 bytes.
- The proportional font can't line up columns, so no rows carry counts
  (Chris, 2026-10-27).
- Text from players or the server goes through `Core_Menu_Clean`: control
  bytes, quotes, backslashes, braces, a leading `*` or `$` are removed.

## Pages

- **Main.** The hostname, gametype and map. Then, while a vote is open, the
  proposal, a blank line, Vote Yes and Vote No, with the cursor on Yes.
  Then, by gametype:
  - team modes: Join Red, Join Blue, Spectate;
  - Duel: Duel Queue;
  - FFA: Join the Match or Spectate;
  - a spectator: Chase Camera or Stop Chasing;
  - in warmup: Ready or Not Ready.

  After those come Match Info, then Call a Vote when you may call one, and
  Close at the foot. A choice that is already the case (your team, your vote answer) is
  drawn in green and skipped by the cursor. Stock has no grey.
- **Match Info.** The gametype, ruleset and map, the time limit, the frag
  or capture limit, and with the match flow on, overtime, ready-up and
  rounds.
- **Call a Vote.** The allowed kinds: Map and Gametype open their lists;
  Restart Match is called from here.
- **Maps.** The vote's map list. The current map is marked and skipped.
- **Gametypes.** A gametype with one ruleset is voted from the list. One with
  variants opens a submenu, for example Duel → Duel / HoonyMode. The current
  one is marked.

See docs/design/voting.md for the vote rules.
