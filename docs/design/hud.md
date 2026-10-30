# HUD

What Axiom adds to stock's deathmatch HUD, and where. Everything is
stock-client compatible: stock layout tokens, stat slots from the free range
(`core/stat_slots*`), text in general configstrings. The bottom HUD (health,
ammo, armor, the weapon icon, pickups) stays exactly stock.

Settled in Chris's playtests (2026-10-26/27). The interactive map of the
screen (all three screen sizes) is in the playtest notes. In short, every
element has one fixed place so players learn where to look.

## Layout tokens

- `xv` / `yv` place relative to a centred 320×240 box (its centre is xv 160,
  yv 120). `xl` / `xr` / `yt` / `yb` place relative to the screen edges.
- At HUD scale 3 on 1080p the screen is 640×360 units; at 1440p and 4K it
  can be 320×240, so anything placed from the edges must also fit there.
- `loc_stat_cstring2` centres a string on xv+160, in green.
  `loc_stat_rstring` ends at x. There is no green left-aligned string.
- `num w stat` right-aligns digits: `xv 118 num 3` puts one digit at xv 152.
- The notify lines (obituaries, chat) sit at the top: `scr_maxlines` (4)
  lines of 10 units, and one more while typing. Centre prints start at
  0.2 × screen height.

## The elements

| Element | Where | Token | Shows |
|---|---|---|---|
| Match status | top middle, just under the 4 notify lines | `xv 0 yt 43 loc_stat_cstring2` | WAITING FOR PLAYERS / TEAMS, READY UP 3/6 (ready / playing, bots always ready; ready-up only), WARMUP (no ready-up: the countdown is about to start), COUNTDOWN, OVERTIME, SUDDEN DEATH, ROUND OVER, MATCH END; nothing in normal live play, except in elimination rounds: `ROUND 3 - RED 3 v 2 BLUE` (lives left per team; Duel `ROUND 3`), the clock then the round's time left (docs/design/clan-arena.md). An open vote replaces it: `VOTE: MAP Q2DM3 - YES 1/3 - 27s`, then `VOTE PASSED` |
| Clock | bottom, half way from the 320 box's edge to the centre, above the stock HUD | `xv -80 yb -61 loc_stat_cstring2` (centred on xv 80) | always: the match length before it starts (0:00 with no timelimit), then the time left (or up from LIVE with no timelimit), 0:00 in sudden death. With the match flow off (`match_flow 0`, stock's rules): the time left on stock's timelimit from the level's start, and no clock with `timelimit 0` |
| Countdown digits | the screen centre, below the crosshair | `xv 118 yv 64 num 3` | the match or round countdown's seconds; hidden otherwise |
| Miniscore | bottom right, where team modes show their team scores | `yb -116` / `yb -83`, faces at `xr -34`, `num 3` at `xr -86` | outside team modes, while the match is in play (not warmup, the countdown or the intermission): the top two players' faces (their skin's 32×32 icon) and scores, the same for everyone (ties to the lower slot). Not MuffMode's "leader and you": with no highlight, a row of your own only repeated the frag counter. No "you" highlight (stock's 28×28 frame doesn't fit a 32×32 face, and pictures can't be scaled); stock's frag counter top right still shows your own score (`core/miniscore*`) |
| Name line | above the clock | `xv 2 yb -71` | the player a spectator chases; in team modes the player under the crosshair (`core/id_view*`). Replaces stock's CHASING / SPECTATOR MODE and left-aligned ID lines |

Code: `Match::HudText` / `CountdownSeconds` (`core/match.*`, unit-tested),
published by `PublishHud` in `core/match_register.cpp`, the statusbar in
`G_InitStatusbar` (`rerelease/g_spawn.cpp`, `[mod]`). Harness tiers 11, 13
and 14 read the statusbar tokens and texts from the game tap.

## Sounds

Stock sounds only: the voice counts down and says FIGHT, a won round plays,
the klaxon (`world/klaxon2.wav`) marks overtime and sudden death, the jingle
(`world/xian1.wav`) plays as the scoreboard comes up, and a called vote plays
`misc/pc_up.wav` (as MuffMode does).

## Gotchas

- The client keeps the statusbar while its string keeps the same length. A
  changed statusbar of the same length isn't picked up until a reload under
  a different one, e.g. switching mode and back.
- The weapon icon (`STAT_HELPICON`) stays at stock's xv 150, where it lines
  up with the ammo icon. The text was moved to fit around it, not the icon.
- Stock draws the pickup name over the helpicon. That's stock behaviour and
  is left as it is.
- Stock draws a layout picture at its own size; the server can't scale it.
  Skin icons are 32×32, the team pictures 24×24, so the miniscore is spaced
  for faces rather than reusing the team rows' spacing.
- The miniscore reuses stock's CTF team-score stats (18..21) outside team
  modes rather than taking 4 of the 10 free slots. It writes them after
  `SetCTFStats`, which fills them in every mode.
- A face takes an image configstring for the rest of the level. At most 32
  distinct faces per level; past that, and for an odd skin name, a row
  shows stock's default `male/grunt` face.
- Obituaries need `cl_notifytime` above 0 on the client. Some installs reset
  it to 0 (fix: `set cl_notifytime 5` in the client's `autoexec.cfg`).
