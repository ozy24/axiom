# Gametype API

Status: **v0, Phase 2a.** This page is the design every gametype (in-tree or
downstream) builds against. Change it in the same PR as the code.

## Shape

There is **one dispatch table**: Rogue's `dm_game_rt DMGame` (`g_local.h`),
which id's code already calls at the right places. Axiom adds a **registry**
of gametypes on top. Each gametype fills the table when it is installed; a
null hook means "stock behaviour". Nothing else dispatches gametype logic.

```
g_gametype (cvar, latched, serverinfo)   e.g. "ffa"
        │  Core_Gametype_Start()  (InitGame, where Rogue called InitGameRules)
        ▼
GametypeDef { id, title, install(dm_game_rt &) }
        │  install fills hooks
        ▼
DMGame  ──► id's call sites (p_client.cpp, g_combat.cpp, g_main.cpp, …)
```

- **Registry:** `core/gametype.h`. The enabled gametypes are listed in CMake
  (`GAME_GAMETYPES`), which generates the table. No static self-registration
  (a static library drops it). A downstream mod adds its own gametypes to the
  list.
- **Selection:** `g_gametype` (neutral name, decision 0004), default `ffa`. An
  unknown id falls back to `ffa` with a console warning.
- **Team gametypes run on stock's team code** (decision 0012). In deathmatch,
  `Core_Gametype_PreInit` (PreInitGame, right after `CTFInit`) sets the stock
  `teamplay` and `ctf` cvars from the selected gametype's `retail_mode`, so
  id's TDM and CTF code runs unchanged. A stock config still works: `ctf 1` or
  `teamplay 1` with `g_gametype ffa` (the default) selects `ctf` / `tdm`.
  Because of that, every built-in ruleset sets `ctf` and `teamplay` too.
- **Stock compatibility:** the stock `gamerules` cvar still works exactly as in
  stock. With it set (Rogue's Tag `2`, DeathBall `3`), Rogue's own
  `InitGameRules` runs untouched and `g_gametype` is ignored; Rogue's modes are
  not registry gametypes (decision 0010).
- **Hooks today** (`dm_game_rt`, all optional): `GameInit`, `PostInitSetup`,
  `ClientBegin`, `SelectSpawnPoint`, `PlayerDeath`, `Score`, `PlayerEffects`,
  `DogTag`, `PlayerDisconnect`, `ChangeDamage`, `ChangeKnockback`,
  `CheckDMRules`.
- **Call-site gates:** id gates every call on `gamerules->integer && DMGame.X`.
  A gametype that installs a hook while `gamerules` is 0 needs that gate
  relaxed to `DMGame.X`, marked `[mod]`. Relax gates one at a time, when a
  gametype first needs them; with every hook null the relaxed gate is
  identical to stock.

## Gametype hooks (beyond DMGame)

`GametypeDef` also carries optional hooks the match flow drives from its
per-frame tick, so they need no further edits to id's code: `frame` (every
deathmatch frame), `match_over` (the gametype's own end condition while LIVE:
it returns the reason, such as `"forfeit"` or `"capturelimit"`, or null),
`match_ended` (once at intermission, whichever rules ended the level) and
`client_command` (the gametype's commands, before `ready`; also `join` /
`spectate` from the menu) and `waiting` (a client not in play who has asked
to play: Duel's queue; kept across a map change, docs/design/joining.md)
and `scoreboard` (its own board under `g_scoreboard 1`, within the byte
budget it's given; stock's clock and intermission line follow it;
docs/design/scoreboard.md). The
`DMGame.ClientBegin` gate is relaxed (`[mod]` in `p_client.cpp`) because Duel
installs it.

## Gametypes

| id | title | hooks | notes |
|---|---|---|---|
| `ffa` | Free For All | none | stock deathmatch; the default |
| `duel` | Duel | `ClientBegin`; `frame`, `match_over`, `match_ended`, `client_command`, `waiting`, `scoreboard` | 1v1 play queue (`gametypes/duel/`) of those who asked (`join`): the front two play, the rest spectate (`queue` lists it); a duellist leaving is a forfeit; the loser rotates to the back. Ruleset `duel` |
| `tdm` | Team Deathmatch | none | stock teamplay (`retail_mode` `teamplay`). The match flow scores it by team frags; fraglimit is on team totals, as stock. Ruleset `tdm` |
| `ctf` | Capture The Flag | `match_over` (capturelimit) | stock CTF (`retail_mode` `ctf`). The match flow scores it by captures; the arena reset sends the flags home. Ruleset `ctf` |

Rogue's Tag and DeathBall remain available through the stock `gamerules` cvar,
outside the registry (decision 0010). Stock Tag crashed on every map; that is
fixed (known id bug #1) and tested by harness tier 4.


## Planned hooks (added when first needed, not before)

Checked against the gametypes Axiom and its downstream mods need, so the API
grows in the right direction:

| Need | Who first | Lands in |
|---|---|---|
| Arena reset (items, projectiles, loadouts, placement) | going LIVE | 2b |
| Death interception ("freeze instead of kill") | Freeze Tag (downstream) | when Freeze Tag is built |
| Per-player gametype data, outside id's structs, not saved | Freeze Tag (Duel keeps its queue in its own module) | when needed |
| Per-frame think | Freeze Tag thaw | when needed |
| Team assignment (CTF-compatible `ctf_team`) — **stock's**: TDM and CTF use id's team code as it is (decision 0012) | TDM, CTF | Phase 3b |
| Scoreboard / HUD stats via the stat allocator (`core/stat_slots.h`, exists: slots 54..63) | Duel, CTF | Phase 3 |
| Lobby metadata (retail mode + display name) — **done**: `GametypeDef::retail_mode`; `Core_Gametype_Start` publishes Q2REX's `q2rex_lobby_*` cvars each game load | Duel | Phase 3a |

Rules for new hooks: plain function pointers and simple types (decision 0005
keeps a plugin loader possible), null means stock, and each call site in id's
code is a marked one-line call.
