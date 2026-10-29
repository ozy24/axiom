# The mod's scoreboards

Outside team modes, `g_scoreboard 1` replaces stock's board: FFA gets
columns (decision 0018), Duel its own board (decision 0019, below). The
built-in `ffa` and `duel` rulesets turn it on; `g_scoreboard 0` brings
stock's back.

## FFA

```
                [2026-10-29 19:40]
    Frg Dth Player           FPH Time Ping
 1   42  15 Xin              379    6   17
 2   39   7 aid              352    6   15
    Spectators
     0  18 Pidarassius        0  20 Player
```

- **Position**: the place on the board (ties take the next number). Your
  own row, pulled in on a full server, shows its real position.

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

## Duel

```
                  Duel on The Edge
                  Match time 10:00            (intermission)
          WINNER
   [face] Xin                [face] aid       stock's player blocks
          10  ping 17               7  ping 15
          W 3  L 1  streak 3        W 0  L 2  each duellist's record

  W  L  Next up
1 0  2  OESD                                   the queue, in order
  Spectators
  0  0  Tab               1  4  bizich
```

- The two duellists in stock's blocks (face, name, score, ping, minutes),
  with a tick by each who is ready during ready-up.
- Under each block, their record this session: wins, losses and a
  streak of two or more. Your own is green.
- **Next up:** the queue in order, numbered, with each player's wins and
  losses.
- Spectators two to a line, under the same W and L.
- At intermission: the match time, and WINNER over the winner's block
  (a name can't go in a line of text: see Limits).
- Records last across `gamemap` (the rotation's map change); a `map`
  (a vote, an admin) reloads the module and starts them again. A forfeit
  isn't counted.

Compared with MMREX's: the queue is numbered and the duellists show their
records too; spectators show their record, not "0 0"; no "1st place with
a score of N" line, which says nothing in a duel.

## Limits (KEX client)

- **1024 bytes.** The client keeps the whole layout in a 1024-byte buffer,
  and each extra column costs about 18 bytes per row (its own `xv` and
  `string`: the KEX font is proportional, so columns can't be lined up
  with spaces). About 8 player rows fit with a spectator or two (8
  players and a spectator: 943 bytes). On a fuller server the board
  shows the top players and your own row, then the spectators that still
  fit; a spectator viewer always keeps room for their own line.
- **Names.** A KEX player's name reaches the server only as a token
  (`##P3`), and the client turns it back into the name only in the
  layout's `ctf` row (tested 2026-10-29). That row draws two numbers, then
  the name, so the name is the third column, and nothing but the `ctf`
  row can show a name: hence no chase target for spectators.

## Implementation

Duel: `gametypes/duel/duel_board.{h,cpp}` (layout and records; unit tests
`tests/test_duel_board.cpp`), fed by `Duel_Scoreboard` in
`duel_register.cpp` through the gametype's `scoreboard` hook. Its first
intermission board goes out a frame before the result is in, so Duel's
`match_ended` resends it (`menutime`). Harness tier 11 checks it.

FFA:

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
