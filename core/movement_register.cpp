// Classic jumps, the engine side (decision 0024): g_classic_jumps, and
// pm_config and its configstring set as each level spawns. The numbers are
// core/movement.cpp.
#include "g_local.h"

#include "core/movement.h"
#include "core/movement_hooks.h"

// The harness (tier 16) reads the configstring by its number.
static_assert(CONFIG_CLASSIC_JUMPS == 12106, "update CONFIG_CLASSIC_JUMPS in scripts/harness/q2harness.py");

void Core_Movement_Spawn()
{
	cvar_t *classic = gi.cvar("g_classic_jumps", "1", CVAR_NOFLAGS);
	pm_config.classic_jumps = deathmatch->integer && classic->integer;
	// Deathmatch only, so single player and coop send nothing stock doesn't.
	if (deathmatch->integer)
		gi.configstring(CONFIG_CLASSIC_JUMPS, pm_config.classic_jumps ? "1" : "0");
}

void Core_Movement_Teleported(edict_t *player, const vec3_t &entry, const vec3_t &angles)
{
	if (!deathmatch->integer || !player->client || gi.cvar("g_teleporter_freeze", "1", CVAR_NOFLAGS)->integer)
		return;
	const MoveVec out =
	    Core_Movement_TeleportVelocity({entry[0], entry[1], entry[2]}, {angles[0], angles[1], angles[2]});
	player->velocity = {out[0], out[1], out[2]};
	player->client->ps.pmove.pm_time = 0;
	player->client->ps.pmove.pm_flags &= ~PMF_TIME_TELEPORT;
}
