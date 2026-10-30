# Map entities

Status: **implemented** (decision 0023). MuffMode's map features, so its
maps (the `mm-` remasters) play here as in MuffMode, and the `.ent`
override file, so a server can change any map without repacking it.
Change this page in the same PR as the code.

**Deathmatch only.** Single player and coop load their maps exactly as
stock: no override file, no keys, no new entities.

## Keys that choose the mode

An entity with one of these keys spawns only where the key says. Stock
ignores them (and spawns everything), which on MuffMode's maps put two
items on one spot: mm-fleshref's Power Screen (`"gametype" "ffa"`) on top
of its Combat Armour (`"not_gametype" "ffa"`).

| Key | Spawns |
|---|---|
| `gametype` | only in the listed gametypes (spaces or commas): `ffa duel tdm ctf`, or MuffMode's `tournament` (Duel) and `team` (TDM) |
| `not_gametype` | in every gametype but those listed |
| `notteam` / `notfree` | not in TDM and CTF / not in FFA and Duel |
| `powerups_on` / `powerups_off` | only with powerups on / only with them off (`g_no_items`, the powerups vote) |
| `no_powerups` | as `powerups_off` (mm-conven's Combat Armour under its Quad; MuffMode spawns both) |
| `ruleset` / `not_ruleset`, `notq2` | MuffMode's rulesets: the mod plays as its Quake II ones (`q2`, `q2re`, `mm`); `notq3a`, `notq3` and the like keep the entity |
| `bfg_on` / `bfg_off` | the BFG is always allowed, so `bfg_off` entities stay out |

Known and ignored, as MuffMode ignores them: `notarena`, `nobots`,
`nohumans`, `author`, `author2`, `remasterer`, `spawnpad`, `arena`,
`noarena`, `no_dm_telepads`. None of them warns.

A gametype the mod doesn't have (`ca`, `freeze`) never matches: an entity
only for Clan Arena stays out of all four.

The log counts what the keys left out: `Entities: 4 left out of ffa by
their keys`.

## Entities

| Entity | Here |
|---|---|
| `ammo_shells_small` `_large`, `ammo_bullets_small` `_large`, `ammo_cells_small` `_large`, `ammo_rockets_small`, `ammo_slugs_small` `_large` | stock's ammo with MuffMode's amount (6/20, 16/100, 20/100, 2, 3/20) and its model, which retail's pak0 carries. No new item types (hard rule 2) |
| `trigger_push` with a `target` | Quake III's jump pad: thrown to peak at the target. The pad's direction and speed are set once the level has spawned, so stock's push does the rest (`Entities: 13 jump pads aimed`). Aimed at the level's `sv_gravity` |
| `target_position` | a point, for jump pads to aim at (as `info_notnull`) |
| `target_teleporter` | a point teleporters send players to (its `angle` faces them) |
| `target_relay` | stock's `trigger_relay` |
| `target_remove_powerups` | when fired, takes the activator's powerups and timers; a tech or flag drops as if they'd died |
| `trigger_teleport` | Rogue's, as stock, but its brush hidden like every other trigger's: stock draws it, trigger texture and all (mm-fleshref's teleporter). `Entities: 1 teleport triggers hidden` |
| worldspawn `no_dm_spawnpads` | no pads drawn under spawn points and teleporter exits |

Not supported: MuffMode's own items (`item_regen` is a new powerup, which
stock clients can't show: hard rule 2; `item_teleporter`), `target_delay`,
`target_print`, `target_push`, the `target_shooter_*` family, `func_bobbing`,
`func_pendulum`, and `misc_teleporter`'s `mins`/`maxs`. Among the
remasters on the playtest rig only work-in-progress maps (`-a1`, `-b1`...)
use them; an unknown classname is skipped with stock's warning.

## The .ent override file

A `.ent` file is a map's whole entity list, the text a BSP carries. With
`maps/<map>.ent` in place, a deathmatch level spawns from it instead of
the BSP's, so a server can move items, add the keys above, or take
anything out, without touching the map.

| Cvar | Default | |
|---|---|---|
| `g_entity_override_load` | `1` | 0: always the BSP's entities |
| `g_entity_override_dir` | `maps` | the directory under the game directory |

`sv saveents` writes the level's entities (the BSP's, or the override in
use) to `<dir>/<map>.ent` as a starting point, and never over a file that
is there already.

- **Where:** with Q2RED's file access the file is read through the
  engine's search path (a pak can carry it). Otherwise, and always for
  `sv saveents`, the plain path `<basedir>/<game>/<dir>/<map>.ent`, from
  the server's working directory when `basedir` is unset, as MuffMode's.
  Q2RED's access writes only records (`.json`, `.txt`, `.csv`, `.log`).
- **Checked first:** under 1 MB, well-formed (`{ "key" "value" ... }`
  blocks) and starting with worldspawn. Else the map's own entities, with
  `Entities: maps/q2dm1.ent not used (the first entity isn't worldspawn);
  the map's own`.
- The log names the file it used: `Entities: maps/q2dm1.ent`.
- MuffMode's 99 `.ent` files work as they are; for the stock maps they add
  ambient sounds, intermission cameras and the keyed item swaps above.

## Implementation

- `core/entities.{h,cpp}`: the rules (the keys, the ammo table, the jump
  pad's throw, the file check). Unit tests: `tests/test_entities.cpp`.
- `core/entities_register.cpp`: the adapter. Hooks, each a marked line:
  `SpawnEntities` (the override before parsing; the jump pads once
  spawned), `ED_ParseField` (a known key, before stock's warning),
  `G_InhibitEntity`, `ED_CallSpawn` (the aliases and
  `target_remove_powerups`), `SP_misc_teleporter_dest` (the pads) and
  `ServerCommand` (`sv saveents`).
- The keys live only while a map spawns: nothing is saved, no save names
  change.
- Harness tier 22 writes a q2dm1 override with every kind of key, the ammo,
  a jump pad and the targets, and checks both FFA and Duel.
