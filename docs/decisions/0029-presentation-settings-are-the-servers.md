# 0029: How the game looks is the server's setting, on by default

**Decided:** 2026-11-16 (Chris, playtesting the release's example
configs: "go with 1").

`g_scoreboard`, `g_frag_messages`, `g_loc`, `g_hud_timer` and
`g_hud_miniscore` are on by default (1, 1, 2, 1, 1) and belong to the
server, not a mode: they are set in `server.cfg`, a mode switch leaves
them alone, and no built-in ruleset sets them. `match_allow_no_humans`
(0) moves the same way: whether bots may play alone describes the
server, not the mode.

**Why:**
- They change what players see and read, never how the game plays, so
  "compiled defaults stay stock" (which protects gameplay) doesn't need
  to cover them.
- On by default, a game hosted from the menu (which runs no ruleset)
  gets the mod's scoreboard and messages, not stock's.
- An admin who dislikes one sets it once, in `server.cfg`, instead of in
  four ruleset files.
- Before, the HUD pair was the server's while the scoreboard, frag
  messages and callouts were reset by every mode switch and turned on by
  every built-in ruleset: two rules for one kind of setting.

**Still per mode:** the map list, limits, the `match_*` match rules, and
gameplay (`g_instagib`, `g_dm_*`, `g_no_*`, `g_disable_items`,
`g_friendly_fire`), which a switch resets (core/rulesets.cpp).

**Single player, coop and Rogue's stock gamerules modes** are untouched:
each setting acts only in deathmatch under a registered gametype.
