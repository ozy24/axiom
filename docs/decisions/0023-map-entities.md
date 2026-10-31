# 0023: MuffMode's map entities, and .ent override files

**Decided:** 2026-10-30 (Chris: "proceed with your recommendations").

MuffMode's maps (the `mm-` remasters) carry keys that choose which modes an
entity spawns in, Quake III jump pads and a few entities stock doesn't
have. Stock ignores the keys, so its maps played wrong here: two items on
one spot (mm-fleshref's Power Screen on its Combat Armour), dead jump pads
(mm-longyd's 13), missing ammo and teleport exits. A server also had no way
to change a map's items without repacking it.

**Decided:**
- The keys (`gametype`, `not_gametype`, `notteam`, `notfree`,
  `powerups_on`, `powerups_off`, MuffMode's ruleset keys) are honoured in
  deathmatch, with MuffMode's names (`tournament`, `team`) as well as the
  mod's gametype ids. The mod counts as MuffMode's Quake II rulesets.
- MuffMode's small and large ammo are stock ammo with its amount and model
  (in retail's pak0): no new item types (hard rule 2). Its other new items
  (`item_regen`, a powerup) are out.
- A `trigger_push` with a target aims at it as Quake III's does; the
  points it aims at and teleports to (`target_position`,
  `target_teleporter`) and `target_relay`, `target_remove_powerups` and
  worldspawn `no_dm_spawnpads` are supported, because the final remasters
  use them. What only work-in-progress maps use is not.
- `.ent` override files, MuffMode's names: `g_entity_override_load` (on by
  default: a file is there only because an admin put it there, and the log
  names it), `g_entity_override_dir`. Deathmatch only. `sv saveents`
  writes a starting file instead of MuffMode's `g_entity_override_save`,
  which wrote one on every map load.
- Not MuffMode's `disable_<classname>` / `replace_<classname>` cvars: one
  per map and classname is the factory-style setup the mod avoids
  (decision 0022's reasoning; admin simplicity). A `.ent` file does the
  same in one place.
- Single player and coop stay stock: no override, no keys, no aliases.

Reading the file reopens decision 0011's "a feature needs to read data that
`exec` can't carry": Q2RED's `LoadFile` when the engine offers it, else a
plain path from `basedir`, as MuffMode reads it on retail. See
docs/design/entities.md.

**Reopen if:** a map the mod wants to support needs one of the left-out
entities, or stock clients gain a way to show new items.
