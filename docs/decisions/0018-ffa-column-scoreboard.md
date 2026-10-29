# 0018: A column scoreboard for FFA

**Decided:** 2026-10-29 (Chris)

Stock's FFA board is two columns of player blocks: a face, the name, the
score and the ping. Chris wanted an OpenFFA-style board with columns
(docs/design/scoreboard.md):

```
                [2026-10-29 19:40]
 Frg Dth Player              FPH Time Ping
  42  15 Xin                 379    6   17
```

1. **The columns are Frg, Dth, Player, FPH, Time and Ping.** Time is whole
   minutes played in this match. Eff% was dropped (Chris): you can work it
   out from Frg and Dth, and it cost about three rows on a full server.
   There's no chase target for spectators (Chris: "drop it").
2. **The name comes third.** The client keeps the layout in 1024 bytes,
   and a KEX name reaches the server only as a token (`##P3`). A test
   board on Chris's KEX client showed that only the layout's `ctf` row
   draws the name: `string`, `loc_string` and its `{}` arguments all show
   `##P0`, and `%s` isn't supported. The `ctf` row draws two numbers, then
   the name. Your own row is green.
3. **A full server shows the top players and your own row.** About 10
   rows fit under the limit; spectators get what's left.
4. **`g_scoreboard`**: compiled default 0 (stock's board); the built-in
   `ffa` ruleset sets 1 (competitive defaults). Chris asked for stock's
   board to stay available behind a cvar. Team modes keep stock's team
   board, and so does Duel for now.

**Reopen if:** the KEX client gets a way to draw a name token inside text,
which would let the name come first and bring back the chase target; or
Duel wants the column board.
