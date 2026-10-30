// Engine-facing part of the map rotation (docs/decisions/0014): the level
// end, the announcement, `maplist`. The rules are core/maps.cpp.
#include "g_local.h"

#include "core/gametype.h"
#include "core/maps.h"
#include "core/maps_hooks.h"

#include <cstring>

edict_t *CreateTargetChangeLevel(const char *map); // rerelease/g_main.cpp

namespace
{
cvar_t *g_map_order; // the play order; g_map_list stays as the admin wrote it
bool announced = false;
gtime_t last_time;
bool begun = false;

void RegisterCvars()
{
	if (g_map_order)
		return;
	g_map_order = gi.cvar("g_map_order", "", CVAR_NOSET); // internal: only the module sets it
}

bool Shuffle()
{
	return g_map_list_shuffle->integer != 0;
}

// Our rotation runs with a list, in deathmatch, outside Rogue's stock
// gamerules modes (decision 0010: those stay exactly stock).
bool Active()
{
	return deathmatch->integer && Core_Gametype_Active() && !Core_Maps_Words(g_map_list->string).empty();
}

// The play order, kept in g_map_order while it still holds the list's maps.
std::vector<std::string> Order()
{
	RegisterCvars();
	const std::vector<std::string> kept = Core_Maps_Words(g_map_order->string);
	const std::vector<std::string> order =
	    Core_Maps_Order(Core_Maps_Words(g_map_list->string), kept, level.mapname, Shuffle(), mt_rand);
	if (Shuffle() && order != kept)
		gi.cvar_forceset("g_map_order", join_strings(order, " ").c_str());
	return order;
}

// The map a level's own exit leads to, as stock's EndDMLevel finds it.
std::string StockNext()
{
	if (level.nextmap[0])
		return level.nextmap;
	if (const edict_t *ent = G_FindByString<&edict_t::classname>(nullptr, "target_changelevel"); ent && ent->map)
		return ent->map;
	return level.mapname;
}

std::string MapList()
{
	const std::vector<std::string> list = Core_Maps_Words(g_map_list->string);
	const std::string next = Core_Maps_NextMap();
	if (list.empty())
		return fmt::format("No map rotation is set (g_map_list): each map leads to its own next one.\n"
		                   "Now: {}. Next: {}.",
		                   level.mapname, next);
	return fmt::format("Map rotation{}: {}\nNow: {}. Next: {}.", Shuffle() ? " (shuffled)" : "",
	                   join_strings(Order(), " "), level.mapname, next);
}
} // namespace

bool Core_Maps_EndLevel()
{
	if (!Active())
		return false;
	const MapsNext next = Core_Maps_Next(Order(), level.mapname, Shuffle());
	if (Shuffle())
		gi.cvar_forceset("g_map_order", join_strings(next.order, " ").c_str()); // a new pass from here
	BeginIntermission(CreateTargetChangeLevel(next.map.c_str()));
	return true;
}

void Core_Maps_Frame()
{
	if (!deathmatch->integer)
		return;
	if (!begun || level.time < last_time)
	{
		announced = false;
		begun = true;
	}
	last_time = level.time;
	if (announced || !level.intermissiontime || !level.changemap)
		return;
	announced = true;
	gi.LocBroadcast_Print(PRINT_HIGH, "Next map: {}\n", level.changemap);
	gi.Com_PrintFmt("Maps: next {}\n", level.changemap);
}

std::string Core_Maps_NextMap()
{
	if (level.intermissiontime && level.changemap)
		return level.changemap;
	if (g_dm_same_level->integer)
		return level.mapname;
	if (level.forcemap[0])
		return level.forcemap;
	if (Active())
		return Core_Maps_Next(Order(), level.mapname, Shuffle()).map;
	return StockNext();
}

bool Core_Maps_ClientCommand(edict_t *ent, const char *cmd)
{
	if (!deathmatch->integer || Q_strcasecmp(cmd, "maplist"))
		return false;
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", MapList().c_str());
	return true;
}

bool Core_Maps_ServerCommand(const char *cmd)
{
	if (Q_strcasecmp(cmd, "maplist"))
		return false;
	gi.Com_PrintFmt("{}\n", MapList());
	return true;
}
