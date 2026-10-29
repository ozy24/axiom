# The column scoreboard

Outside team modes, `g_scoreboard 1` replaces stock's FFA board with
columns (decision 0018). The built-in `ffa` ruleset turns it on.

```
                [2026-10-29 19:40]
 Frg Dth Player              FPH Time Ping
  42  15 Xin                 379    6   17
  39   7 aid                 352    6   15
 Spectators
  0  18 Pidarassius           0  20 Player
```

- **Frg**: the score. **Dth**: deaths while the match is in play (warmup
  is free play, so neither counts there).
- **FPH**: frags per hour played. **Time**: whole minutes played in this
  match. Both count from when the match goes live and pause while the
  player spectates.
- **Ping**, capped at 999 as the client does.
- The header is the server's local date and time. Stock's frag limit, its
  clock and the intermission's "press a button" line stay as they were.
- Your own row is green. Players are ordered by frags, then fewer deaths.
- Spectators follow, two to a line, showing score and ping.

## Limits (KEX client)

- **1024 bytes.** The client keeps the whole layout in a 1024-byte buffer,
  and each extra column costs about 18 bytes per row (its own `xv` and
  `string`: the KEX font is proportional, so columns can't be lined up
  with spaces). About 10 player rows fit. On a fuller server the board
  shows the top players and your own row, then the spectators that still
  fit.
- **Names.** A KEX player's name reaches the server only as a token
  (`##P3`), and the client turns it back into the name only in the
  layout's `ctf` row (tested 2026-10-29). That row draws two numbers, then
  the name, so the name is the third column, and nothing but the `ctf`
  row can show a name: hence no chase target for spectators.

## Implementation

- `core/scoreboard.{h,cpp}`: the layout text, ordering, FPH and the fit to
  the budget (unit tests: `tests/test_scoreboard.cpp`).
- `core/scoreboard_register.cpp`: counts deaths (`player_die`) and time
  played (every frame while in play), resets both as the match goes live
  or when a new client takes the slot, and builds each viewer's board.
- Hooks: `DeathmatchScoreboardMessage` (stock's rows give way; its frag
  limit, clock and intermission line stay), `player_die`, and
  `Core_Match_Frame`.
- Harness tier 16 checks the board in a live FFA match, including a
  suicide counted as a death.
