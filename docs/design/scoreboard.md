# The mod's scoreboards

`g_scoreboard 1` gives the mod's boards: FFA gets columns (decision 0018),
Duel its own board (decision 0019, below), and the team boards labelled
spectators. Every built-in ruleset turns it on; `g_scoreboard 0` brings
stock's back.

**Spectators**, on every board: a KEX name shows only in a `ctf` row, which
always draws two numbers before it (see Limits), so a spectator's row can't
be the name alone. Its numbers are 0 and ping, under a heading of stock's
ping icon (`picn ping`, the 9x9 the duel blocks show) and `Spectators`, so
they never read as frags and deaths (Chris, 2026-10-30).

## FFA

```
    Frg Dth Player           FPH Time Ping
 1   42  15 Xin              379    6   17
 2   39   7 aid              352    6   15
       (ping) Spectators
     0  18  Pidarassius       0   20  Player
```

- **Position**: the place on the board (ties take the next number). Your
  own row, pulled in on a full server, shows its real position.

- **Frg**: the score. **Dth**: deaths while the match is in play (warmup
  is free play, so neither counts there).
- **FPH**: frags per hour played. **Time**: whole minutes played in this
  match. Both count from when the match goes live and pause while the
  player spectates.
- **Ping**, capped at 999 as the client does.
- No date line, and the board starts a row under stock's frag limit and
  clock line (Chris, 2026-10-30). Stock's frag limit, its clock and the
  intermission's "press a button" line stay as they were.
- About 8 or 9 rows fit, players and spectators together: the 1024-byte
  layout, not the screen, is the limit (8 with three-digit frags, FPH and
  ping).
- Your own row is green. Players are ordered by frags, then fewer deaths.
- Spectators follow, two to a line: 0 and ping.
- In ready-up, a green `R` right of Ping on each ready player's row. Bots
  are always ready. Rows and spectators are fitted first; a mark only goes
  in while the 1024 bytes allow.

## Duel

```
                  Duel on The Edge
                  Match time 10:00            (intermission)
          WINNER
   [face] Xin                [face] aid       stock's player blocks
          10  ping 17               7  ping 15

     (ping) Queue
  1  17   OESD                                 the queue, in order
     (ping) Spectators
  0  20   Tab             0  45   bizich
```

- The two duellists in stock's blocks (face, name, score, ping, minutes),
  with a tick by each who is ready during ready-up.
- **Queue:** in order, each row its place and ping (yours green).
- Spectators two to a line: 0 and ping.
- No win/loss records (Chris, 2026-10-30): nobody reads them, and they
  started again on every `map`.
- At intermission: the match time, and WINNER over the winner's block
  (a name can't go in a line of text: see Limits).
Compared with MMREX's: the queue is numbered; no "1st place with a score
of N" line, which says nothing in a duel.

## TDM and CTF

Stock's team board, spectators labelled as above (two marked hooks in
CTFScoreboardMessage, under `g_scoreboard 1`), with two marks left of a row: the captain's `*`
(decision 0016) and, in ready-up, a green `R` for a ready player (x -56 on
RED, 182 on BLUE: where MuffMode puts its tick). The marks are added once
stock's rows and spectators are in (`Core_Scoreboard_TeamReady`, one marked
call), so on a full board a mark is dropped before a player is.

Not MuffMode's tick picture: a stock client draws `wheel/p_compass_selected`
at its 24x24 (MuffMode's own client code shrinks it to 12x12), three rows
high, covering the score (playtest 2026-10-29). Duel's board keeps the
tick: its player blocks have room for it.

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

Duel: `gametypes/duel/duel_board.{h,cpp}` (the layout; unit tests
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
