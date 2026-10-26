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

## Gametypes

| id | title | hooks | notes |
|---|---|---|---|
| `ffa` | Free For All | none | stock deathmatch; the default |

Rogue's Tag and DeathBall remain available through the stock `gamerules` cvar,
outside the registry (decision 0010). Stock Tag crashed on every map; that is
fixed (known id bug #1) and tested by harness tier 4.

TDM, CTF and Duel arrive in Phase 3; TDM and CTF are currently the stock
`teamplay` / `ctf` cvars and keep working as before.

## Planned hooks (added when first needed, not before)

Checked against the gametypes Axiom and its downstream mods need, so the API
grows in the right direction:

| Need | Who first | Lands in |
|---|---|---|
| Rounds (end on frags / time / elimination / condition), respawn policy | Duel (HoonyMode), Clan Arena ruleset | Phase 2b match state machine |
| Arena reset (items, projectiles, loadouts, placement) | rounds | 2b |
| Death interception ("freeze instead of kill") | Freeze Tag (downstream) | when Freeze Tag is built |
| Per-player gametype data, outside id's structs, not saved | Duel queue, Freeze Tag | Phase 3a |
| Per-frame think | Freeze Tag thaw | when needed |
| Team assignment (CTF-compatible `ctf_team`) | TDM, CTF | Phase 3b team service |
| Scoreboard / HUD stats via the stat allocator | Duel, CTF | Phase 3 |
| Lobby metadata (retail mode + display name) | Duel | Phase 3a / 4 |

Rules for new hooks: plain function pointers and simple types (decision 0005
keeps a plugin loader possible), null means stock, and each call site in id's
code is a marked one-line call.
