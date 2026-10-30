// Engine-facing warmup loadout (core/warmup_hooks.h): the map's weapons, with
// full ammo, for everyone spawning in warmup; no drops when they die.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match_hooks.h"
#include "core/warmup_hooks.h"

#include <string>
#include <vector>

void NoAmmoWeaponChange(edict_t *ent, bool sound); // id's, in p_weapon.cpp

namespace
{
cvar_t *match_warmup_weapons;
bool announced = false; // this level's loadout went to the log
gtime_t last_time;

void RegisterCvars()
{
	if (match_warmup_weapons)
		return;
	// 1: in warmup, every weapon the map places, with full ammo.
	match_warmup_weapons = gi.cvar("match_warmup_weapons", "1", CVAR_NOFLAGS);
}

// In warmup, with the loadout on, a client spawning into play (not a
// spectator; in team modes, on a team). Not Core_Match_Playing: a first
// spawn isn't marked spawned yet.
bool On(const edict_t *ent)
{
	if (!deathmatch->integer || !Core_Gametype_Active() || !ent->client || !Core_Match_Warmup())
		return false;
	RegisterCvars();
	return match_warmup_weapons->integer && !ent->client->resp.spectator &&
	       (!G_TeamplayEnabled() || ent->client->resp.ctf_team != CTF_NOTEAM);
}

// The weapons the map places (not ones players dropped), each once, in item order.
std::vector<gitem_t *> MapWeapons()
{
	std::vector<bool> seen(IT_TOTAL);
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
	{
		const edict_t *e = g_edicts + i;
		if (e->inuse && e->item && (e->item->flags & IF_WEAPON) && !e->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
			seen[e->item->id] = true;
	}
	std::vector<gitem_t *> out;
	for (int id = 0; id < IT_TOTAL; ++id)
		if (seen[id])
			out.push_back(GetItemByIndex((item_id_t)id));
	return out;
}
} // namespace

void Core_Warmup_Loadout(edict_t *ent)
{
	if (!On(ent) || level.intermissiontime)
		return;
	const std::vector<gitem_t *> weapons = MapWeapons();
	for (gitem_t *w : weapons)
	{
		ent->client->pers.inventory[w->id] = std::max<int>(ent->client->pers.inventory[w->id], 1);
		if (gitem_t *ammo = w->ammo ? GetItemByIndex(w->ammo) : nullptr;
		    ammo && ammo->tag >= AMMO_BULLETS && ammo->tag < AMMO_MAX)
			ent->client->pers.inventory[ammo->id] = ent->client->pers.max_ammo[ammo->tag];
	}
	if (!weapons.empty())
	{
		// The best of them in hand: stock raises pers.weapon next.
		NoAmmoWeaponChange(ent, false);
		if (ent->client->newweapon)
			ent->client->pers.weapon = ent->client->newweapon;
	}
	if (level.time < last_time) // a new level
		announced = false;
	last_time = level.time;
	if (!announced)
	{
		announced = true;
		std::string names;
		for (const gitem_t *w : weapons)
			names += std::string(names.empty() ? "" : ", ") + w->classname;
		gi.Com_PrintFmt("Warmup: loadout {}\n", names.empty() ? "none" : names);
	}
}

bool Core_Warmup_KeepsWeapons(const edict_t *ent)
{
	return On(ent);
}
