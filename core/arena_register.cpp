// Engine-facing part of the arena rules (core/arena.h): the cvars, the
// loadout at spawn, the item filter, the self-damage rule and the damage
// score.
#include "g_local.h"

#include "core/arena.h"
#include "core/arena_hooks.h"
#include "core/gametype.h"
#include "core/match_hooks.h"

namespace
{
cvar_t *match_arena, *match_arena_health, *match_arena_armor, *match_damage_score, *g_dm_no_self_damage;
DamageScore damage_score;
gtime_t last_time;

// SpawnItem runs before any frame: the cvars register on first use.
void RegisterCvars()
{
	if (match_arena)
		return;
	match_arena = gi.cvar("match_arena", "0", CVAR_NOFLAGS);
	match_arena_health = gi.cvar("match_arena_health", "150", CVAR_NOFLAGS);
	match_arena_armor = gi.cvar("match_arena_armor", "150", CVAR_NOFLAGS);
	match_damage_score = gi.cvar("match_damage_score", "0", CVAR_NOFLAGS);
	// MuffMode's name: it does the same (decision 0004).
	g_dm_no_self_damage = gi.cvar("g_dm_no_self_damage", "0", CVAR_NOFLAGS);
}
} // namespace

bool Core_Arena_On()
{
	RegisterCvars();
	return deathmatch->integer && Core_Gametype_Active() && match_arena->integer;
}

void Core_Arena_Loadout(edict_t *ent)
{
	if (!Core_Arena_On() || level.intermissiontime || ent->client->resp.spectator)
		return;
	const ArenaLoadout l = Core_Arena_Loadout(match_arena_health->integer, match_arena_armor->integer);
	gclient_t *cl = ent->client;
	for (const std::string &name : l.weapons)
	{
		gitem_t *w = FindItemByClassname(name.c_str());
		if (!w)
			continue;
		cl->pers.inventory[w->id] = 1;
		if (gitem_t *ammo = w->ammo ? GetItemByIndex(w->ammo) : nullptr;
		    ammo && ammo->tag >= AMMO_BULLETS && ammo->tag < AMMO_MAX)
			cl->pers.inventory[ammo->id] = cl->pers.max_ammo[ammo->tag];
	}
	cl->pers.inventory[IT_ARMOR_JACKET] = cl->pers.inventory[IT_ARMOR_COMBAT] = 0;
	cl->pers.inventory[IT_ARMOR_BODY] = l.armor;
	ent->health = ent->max_health = l.health;
	cl->pers.health = cl->pers.max_health = l.health;
	if (gitem_t *hand = FindItemByClassname(l.in_hand.c_str()))
		cl->pers.weapon = hand; // stock raises pers.weapon next
}

bool Core_Arena_NoItem(const gitem_t *item)
{
	return Core_Arena_On() && !(item->flags & IF_KEY) && item->id != IT_FLAG1 && item->id != IT_FLAG2;
}

bool Core_Arena_NoSelfDamage(const edict_t *targ, const edict_t *attacker, int dflags)
{
	RegisterCvars();
	return deathmatch->integer && g_dm_no_self_damage->integer && targ == attacker && targ->client &&
	       !(dflags & DAMAGE_NO_PROTECTION);
}

void Core_Arena_Damage(edict_t *targ, edict_t *attacker, int take, int psave, int asave)
{
	RegisterCvars();
	if (level.time < last_time) // a new level
		damage_score.Reset();
	last_time = level.time;
	if (!deathmatch->integer || !match_damage_score->integer || !Core_Match_RoundInPlay() || !targ->client ||
	    !attacker->client || targ == attacker || targ->health <= 0 || OnSameTeam(targ, attacker))
		return;
	const int points = damage_score.Add((int)(attacker - g_edicts), take, targ->health, psave + asave);
	attacker->client->resp.score += points;
}

void Core_Arena_ResetDamage()
{
	damage_score.Reset();
}

void Core_Arena_Disconnect(edict_t *ent)
{
	damage_score.Forget((int)(ent - g_edicts));
}
