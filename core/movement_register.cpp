// Classic jumps, the engine side (decision 0024): g_classic_jumps, and
// pm_config and its configstring set as each level spawns. The numbers are
// core/movement.cpp.
#include "g_local.h"

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
