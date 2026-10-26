// Engine-facing part of the gametype registry: selection from cvars.
#include "g_local.h"

#include "core/gametype.h"

namespace
{
const GametypeDef *active = nullptr;
} // namespace

const GametypeDef *Core_Gametype_Active()
{
	return active;
}

void Core_Gametype_Start()
{
	cvar_t *g_gametype = gi.cvar("g_gametype", "ffa", CVAR_SERVERINFO | CVAR_LATCH);

	// Single player, coop, and Rogue's stock modes (gamerules 2 Tag,
	// 3 DeathBall): exactly what stock InitGame did (decision 0010).
	if (!deathmatch->integer || gamerules->integer)
	{
		if (gamerules->integer)
		{
			InitGameRules();
			if (deathmatch->integer)
				gi.Com_PrintFmt("Gametype: stock gamerules {}\n", gamerules->integer);
		}
		return;
	}

	const GametypeDef *def = Core_Gametype_Find(core_gametypes, core_gametype_count, g_gametype->string);
	if (!def)
	{
		gi.Com_PrintFmt("Unknown g_gametype \"{}\"; using ffa.\n", g_gametype->string);
		def = Core_Gametype_Find(core_gametypes, core_gametype_count, "ffa"); // always present (CMakeLists.txt)
	}
	active = def;
	if (Q_strcasecmp(g_gametype->string, def->id))
		gi.cvar_forceset("g_gametype", def->id);

	if (def->install)
	{
		def->install(DMGame);
		if (DMGame.GameInit)
			DMGame.GameInit();
	}
	gi.Com_PrintFmt("Gametype: {} ({})\n", def->title, def->id);
}
