// Engine-facing part of the gametype registry: selection from cvars.
#include "g_local.h"

#include "core/gametype.h"
#include "core/rulesets.h"

#include <string>
#include <utility>

namespace
{
const GametypeDef *active = nullptr;

// Q2REX's optional lobby contract (source/include/quake2ex_lobby_discovery.h):
// the engine clears these before each game load and the module republishes
// them. Q2REX browsers show the mode name; retail browsers still see
// retail_mode's stock label. Other engines just carry the cvars, unused.
void PublishLobbyMode(const GametypeDef &def)
{
	const std::pair<const char *, const char *> values[] = {
	    {"q2rex_lobby_mode", def.id},
	    {"q2rex_lobby_mode_name", def.title},
	    {"q2rex_lobby_retail_mode", def.retail_mode},
	    {"q2rex_lobby_factory", ""},
	    {"q2rex_lobby_score_kind", "frags"},
	};
	for (const auto &[cvar, value] : values)
	{
		gi.cvar(cvar, "", CVAR_NOSET);
		gi.cvar_forceset(cvar, value);
	}
}
} // namespace

void Core_Gametype_PreInit()
{
	// Single player and coop never select a gametype, nor do Rogue's stock
	// modes. ctf or teamplay without deathmatch is still a match: stock forces
	// deathmatch on for them, right after this.
	if (!deathmatch->integer && !ctf->integer && !teamplay->integer)
		return;
	if (gi.cvar("gamerules", "0", CVAR_LATCH)->integer)
		return;
	Core_Rulesets_PreInit(); // server.cfg's starting mode (docs/decisions/0013)
	cvar_t *g_gametype = gi.cvar("g_gametype", "ffa", CVAR_SERVERINFO | CVAR_LATCH);
	const GametypeDef *def = Core_Gametype_Find(core_gametypes, core_gametype_count, g_gametype->string);
	if (!def || !strcmp(def->id, "ffa"))
	{
		// A stock config: ctf / teamplay pick the gametype built on them.
		const char *stock = ctf->integer ? "ctf" : teamplay->integer ? "teamplay" : nullptr;
		for (size_t i = 0; stock && i < core_gametype_count; ++i)
		{
			if (!strcmp(core_gametypes[i]->retail_mode, stock))
			{
				gi.Com_PrintFmt("{} 1 selects g_gametype {}\n", stock, core_gametypes[i]->id);
				gi.cvar_forceset("g_gametype", core_gametypes[i]->id);
				def = core_gametypes[i];
				break;
			}
		}
		if (!def)
			return; // unknown: Core_Gametype_Start reports it and uses ffa
	}
	else if (!deathmatch->integer)
		return; // an explicit gametype only applies to deathmatch
	gi.cvar_forceset("ctf", strcmp(def->retail_mode, "ctf") ? "0" : "1");
	gi.cvar_forceset("teamplay", strcmp(def->retail_mode, "teamplay") ? "0" : "1");
}

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

	DMGame = {}; // nothing left from a gametype before (the module can outlive one)
	if (def->install)
	{
		def->install(DMGame);
		if (DMGame.GameInit)
			DMGame.GameInit();
	}
	gi.Com_PrintFmt("Gametype: {} ({})\n", def->title, def->id);
	PublishLobbyMode(*def);
}
