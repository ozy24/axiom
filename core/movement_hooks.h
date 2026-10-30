// Classic jumps' hook into id's code (decision 0024; core/movement.h).
#pragma once

struct edict_t;
struct vec3_t;

// From SpawnEntities, beside the air acceleration: classic jumps on for
// this level in deathmatch with g_classic_jumps 1 (the default), for the
// server's pmove and, through a configstring, the client's prediction.
// Single player and coop stay stock. A change applies from the next map.
void Core_Movement_Spawn();

// From the teleporters (misc_teleporter, Rogue's trigger_teleport), after
// stock stops the player: in deathmatch with g_teleporter_freeze 0, sends
// them out of the exit facing its `angles` at the speed they went in with
// (`entry`), with no hold. g_teleporter_freeze 1 (the default) is stock's.
void Core_Movement_Teleported(edict_t *player, const vec3_t &entry, const vec3_t &angles);
