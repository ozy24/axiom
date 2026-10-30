// Map entities' hooks into id's code (docs/design/entities.md), each a
// marked one-line call. All of it is deathmatch only: single player and coop
// load their maps exactly as stock. The rules are core/entities.h.
#pragma once

struct edict_t;

// From SpawnEntities, before it parses: the entities the level spawns from.
// In deathmatch with g_entity_override_load 1, a valid <dir>/<map>.ent
// (g_entity_override_dir, "maps") replaces the BSP's; else `entities`.
const char *Core_Entities_Override(const char *mapname, const char *entities);

// From ED_ParseField, for a key stock doesn't know: true if it's one of the
// keys core/entities.h knows (kept for Core_Entities_Inhibit, no warning).
bool Core_Entities_Key(edict_t *ent, const char *key, const char *value);

// From G_InhibitEntity, for every entity after the world: true when its
// keys keep it out of this level's mode.
bool Core_Entities_Inhibit(edict_t *ent);

// From ED_CallSpawn, before stock looks the classname up: MuffMode's small
// and large ammo become stock ammo, target_relay stock's trigger_relay,
// target_position and target_teleporter points; true when it spawned an
// entity of its own (target_remove_powerups).
bool Core_Entities_Spawn(edict_t *ent);

// From SpawnEntities once every entity has spawned: aims the jump pads (a
// trigger_push with a target) and logs what the keys left out.
void Core_Entities_Spawned();

// From SP_misc_teleporter_dest: the map's worldspawn asked for no pads
// under spawn points and teleporter exits (no_dm_spawnpads).
bool Core_Entities_NoSpawnPads();

// `sv saveents`: writes the level's entities to <dir>/<map>.ent, a start
// for an override. True if handled.
bool Core_Entities_ServerCommand(const char *cmd);
