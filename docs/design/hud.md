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
| Match status | top middle, just under the 4 notify lines | `xv 0 yt 43 loc_stat_cstring2` | WAITING FOR PLAYERS / TEAMS, WARMUP, COUNTDOWN, OVERTIME, SUDDEN DEATH, ROUND OVER, MATCH END; nothing in normal live play. An open vote replaces it: `VOTE: MAP Q2DM3 - YES 1/3 - 27s`, then `VOTE PASSED` |
| Clock | bottom, half way from the 320 box's edge to the centre, above the stock HUD | `xv -80 yb -61 loc_stat_cstring2` (centred on xv 80) | always: the match length before it starts (0:00 with no timelimit), then the time left (or up from LIVE with no timelimit), 0:00 in sudden death |
| Countdown digits | the screen centre, below the crosshair | `xv 118 yv 64 num 3` | the match or round countdown's seconds; hidden otherwise |
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
- Obituaries need `cl_notifytime` above 0 on the client. Some installs reset
  it to 0 (fix: `set cl_notifytime 5` in the client's `autoexec.cfg`).
