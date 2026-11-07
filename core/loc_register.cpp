// Engine-facing part of the location callouts (core/loc.h, decision 0027):
// the level's .loc file, what the caller sees and carries, and who hears it.
#include "g_local.h"

#include "core/host_files.h"
#include "core/loc.h"
#include "core/loc_hooks.h"
#include "core/match_hooks.h"
#include "core/player_name.h"

namespace
{
constexpr size_t kMaxFile = 1 << 20;

std::vector<LocPlace> places;

// 0 off; 1 the map's .loc file; 2 that, or with none the nearest item.
cvar_t *LocCvar()
{
	return gi.cvar("g_loc", "0", CVAR_NOFLAGS);
}

LocVec Vec(const vec3_t &v)
{
	return {v.x, v.y, v.z};
}

vec3_t Eye(const edict_t *ent)
{
	return ent->s.origin + vec3_t{0, 0, static_cast<float>(ent->viewheight)};
}

bool Landmark(const edict_t *e)
{
	if (!e->inuse || !e->item || e->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER))
		return false;
	const item_flags_t flags = e->item->flags;
	return ((flags & IF_WEAPON) && !(flags & IF_AMMO)) || (flags & IF_POWERUP) || e->item->id == IT_HEALTH_MEGA;
}

// The nearest weapon, powerup or mega health in the caller's PVS.
std::string ItemPlace(edict_t *ent)
{
	const vec3_t eye = Eye(ent);
	const edict_t *best = nullptr;
	float best_d = 0;
	for (uint32_t i = 1; i < globals.num_edicts; ++i)
	{
		const edict_t *e = g_edicts + i;
		if (!Landmark(e) || !gi.inPVS(eye, e->s.origin, false))
			continue;
		const float d = (e->s.origin - ent->s.origin).lengthSquared();
		if (!best || d < best_d)
		{
			best = e;
			best_d = d;
		}
	}
	if (!best)
		return ent->waterlevel > WATER_NONE ? "water" : level.mapname;
	std::vector<float> twins;
	for (uint32_t i = 1; i < globals.num_edicts; ++i)
	{
		const edict_t *e = g_edicts + i;
		if (e != best && Landmark(e) && e->item->id == best->item->id)
			twins.push_back(e->s.origin.z);
	}
	return Core_Loc_ItemLabel(Core_Loc_ItemName(best->item->classname), best->s.origin.z, twins);
}

bool Place(edict_t *ent, std::string &out)
{
	const vec3_t eye = Eye(ent);
	const int i = Core_Loc_Pick(places, Vec(ent->s.origin),
	                            [&](const LocPlace &p)
	                            {
		                            const vec3_t to{p.origin.x, p.origin.y, p.origin.z};
		                            return gi.traceline(eye, to, ent, MASK_SOLID).fraction == 1.0f;
	                            });
	if (i >= 0)
	{
		out = places[i].label;
		return true;
	}
	if (LocCvar()->integer < 2)
		return false;
	out = ItemPlace(ent);
	return true;
}

// The players `ent` can see in front of it, alive and in play: its team's
// (`team`) or everyone's.
std::string InView(edict_t *ent, bool team)
{
	const vec3_t eye = Eye(ent);
	std::vector<std::string> names;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *other = g_edicts + 1 + i;
		if (other == ent || !other->inuse || !other->client || other->health <= 0 || !Core_Match_Playing(other))
			continue;
		if (team && !OnSameTeam(ent, other))
			continue;
		if (!Core_Loc_InView(Vec(eye), Vec(ent->client->v_forward), Vec(Eye(other))) || !visible(ent, other))
			continue;
		names.push_back(Core_PlayerName(other));
	}
	return Core_Loc_Names(names);
}

LocValues Values(edict_t *ent, const std::string &text, const std::string &place)
{
	gclient_t *cl = ent->client;
	LocValues v;
	v.place = place;
	v.health = std::max(0, ent->health);
	const item_id_t armor = ArmorIndex(ent);
	const bool power = cl->pers.inventory[IT_ITEM_POWER_SCREEN] > 0 || cl->pers.inventory[IT_ITEM_POWER_SHIELD] > 0;
	v.armor = Core_Loc_ArmorText(armor ? cl->pers.inventory[armor] : 0, power ? cl->pers.inventory[IT_AMMO_CELLS] : -1);
	v.weapon = v.ammo = "none";
	if (const gitem_t *w = cl->pers.weapon)
	{
		const std::string name = Core_Loc_ItemName(w->classname);
		v.weapon = w->ammo != IT_NULL ? name + ":" + std::to_string(cl->pers.inventory[w->ammo]) : name;
		if (const gitem_t *ammo = w->ammo != IT_NULL ? GetItemByIndex(w->ammo) : nullptr)
			v.ammo = Core_Loc_ItemName(ammo->classname);
	}
	if (Core_Loc_Uses(text, 'n'))
		v.team = InView(ent, true);
	if (Core_Loc_Uses(text, 'N'))
		v.all = InView(ent, false);
	return v;
}
} // namespace

void Core_Loc_LevelStart()
{
	places.clear();
	LocCvar();
	if (!deathmatch->integer)
		return;
	const char *dir = gi.cvar("g_loc_dir", "locs", CVAR_NOFLAGS)->string;
	std::string text, where;
	if (!Core_ReadGameFile(dir, std::string(level.mapname) + ".loc", text, where))
		return;
	if (text.size() > kMaxFile)
	{
		gi.Com_PrintFmt("Loc: {} not used (over 1 MB)\n", where.c_str());
		return;
	}
	size_t skipped = 0;
	places = Core_Loc_ParseFile(text, skipped);
	const std::string note = skipped ? ", " + std::to_string(skipped) + " lines skipped" : "";
	gi.Com_PrintFmt("Loc: {} places from {}{}\n", places.size(), where.c_str(), note.c_str());
}

bool Core_Loc_ClientCommand(edict_t *ent, const char *cmd)
{
	if (!deathmatch->integer || Q_strcasecmp(cmd, "loc") != 0)
		return false;
	if (!LocCvar()->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Location callouts are off on this server.\n");
		return true;
	}
	const std::string text = Core_Loc_Text(gi.args());
	if (text.empty())
	{
		gi.LocClient_Print(
		    ent, PRINT_HIGH,
		    "Usage: loc <text with %l place, %h health, %a armor, %w weapon, %m ammo, %n teammates or %N "
		    "players in view>\n");
		return true;
	}
	std::string place;
	if (!Place(ent, place))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No locations for this map.\n");
		return true;
	}
	if (CheckFlood(ent))
		return true;

	const std::string body = Core_Loc_Expand(text, Values(ent, text, place));
	// As chat: team games to the caller's team (spectators to spectators),
	// else to everyone; the name a whole argument (the client decodes its
	// token only as one), the same bases as CTFSay_Team and Q2RED's chat.
	const bool teams = G_TeamplayEnabled();
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *other = g_edicts + 1 + i;
		if (!other->inuse || !other->client)
			continue;
		if (teams && other->client->resp.ctf_team != ent->client->resp.ctf_team)
			continue;
		gi.LocClient_Print(other, PRINT_CHAT, teams ? "({}): {}\n" : "{}: {}\n", ent->client->pers.netname,
		                   body.c_str());
	}
	const std::string who = teams ? "(" + Core_PlayerName(ent) + ")" : Core_PlayerName(ent);
	gi.Com_PrintFmt("Loc: {}: {}\n", who.c_str(), body.c_str());
	return true;
}
