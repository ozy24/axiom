// Engine-facing part of the gametype registry: selection from cvars.
#include "g_local.h"

#include "core/gametype.h"

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
	const bool rounds = gi.cvar("match_rounds", "0", CVAR_NOFLAGS)->integer != 0;
	const std::string name = rounds ? std::string(def.title) + " (rounds)" : def.title;
	const std::pair<const char *, const char *> values[] = {
	    {"q2rex_lobby_mode", def.id},
	    {"q2rex_lobby_mode_name", name.c_str()},
	    {"q2rex_lobby_retail_mode", def.retail_mode},
	    {"q2rex_lobby_factory", ""},
	    {"q2rex_lobby_score_kind", rounds ? "rounds" : "frags"},
	};
	for (const auto &[cvar, value] : values)
	{
		gi.cvar(cvar, "", CVAR_NOSET);
		gi.cvar_forceset(cvar, value);
	}
}
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
	PublishLobbyMode(*def);
}
