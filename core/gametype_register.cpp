// Engine-facing part of the gametype registry: selection from cvars.
#include "g_local.h"

#include "core/gametype.h"

static_assert(RDM_TAG == 2 && RDM_DEATHBALL == 3, "gametypes/tag and gametypes/ball hard-code these");

namespace
{
const GametypeDef *active = nullptr;

const GametypeDef &Ffa()
{
	const GametypeDef *ffa = Core_Gametype_Find(core_gametypes, core_gametype_count, "ffa");
	return *ffa; // ffa is always in the registry (CMakeLists.txt checks)
}
} // namespace

const GametypeDef &Core_Gametype_Active()
{
	return active ? *active : Ffa();
}

void Core_Gametype_Start()
{
	cvar_t *g_gametype = gi.cvar("g_gametype", "ffa", CVAR_SERVERINFO | CVAR_LATCH);

	// Single player and coop: exactly what stock InitGame did.
	if (!deathmatch->integer)
	{
		if (gamerules->integer)
			InitGameRules();
		return;
	}

	// The stock gamerules cvar still selects Rogue's modes, so existing server
	// configs keep working; otherwise g_gametype decides.
	const GametypeDef *def = Core_Gametype_ForRogueGamerules(core_gametypes, core_gametype_count, gamerules->integer);
	if (!def)
	{
		def = Core_Gametype_Find(core_gametypes, core_gametype_count, g_gametype->string);
		if (!def)
		{
			gi.Com_PrintFmt("Unknown g_gametype \"{}\"; using ffa.\n", g_gametype->string);
			def = &Ffa();
		}
	}
	active = def;

	if (gamerules->integer != def->rogue_gamerules)
		gi.cvar_forceset("gamerules", G_Fmt("{}", def->rogue_gamerules).data());
	if (Q_strcasecmp(g_gametype->string, def->id))
		gi.cvar_forceset("g_gametype", def->id);

	if (gamerules->integer)
		InitGameRules(); // Rogue fills DMGame and runs its GameInit
	if (def->install)
	{
		def->install(DMGame);
		if (DMGame.GameInit)
			DMGame.GameInit();
	}
	gi.Com_PrintFmt("Gametype: {} ({})\n", def->title, def->id);
}
