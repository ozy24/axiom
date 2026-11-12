// Map entities beyond stock (docs/design/entities.md), as pure logic: the
// keys that choose which modes an entity spawns in, MuffMode's small and
// large ammo, Quake III's aimed jump pads and the checks on a .ent override
// file. The adapter is entities_register.cpp.
//
// The keys and entities are MuffMode's, so its maps (mm-fleshref's Power
// Screen in FFA, Combat Armour in the other modes) play here as there.
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The mode a level spawns in, as the keys see it.
struct EntitySpawnMode
{
	std::string gametype = "ffa"; // the gametype id: ffa, duel, tdm, ctf
	bool teams = false;           // TDM, CTF
	bool powerups = true;         // g_no_items 0
};

using EntityKeys = std::vector<std::pair<std::string, std::string>>;

// A key this file knows (case-insensitive), kept for Core_Entities_Excluded
// instead of stock's "is not a valid field".
bool Core_Entities_IsKey(std::string_view key);

// Whether an entity with these keys stays out of a level in `mode`:
// - gametype / not_gametype: a list (spaces or commas) of gametype ids or
//   MuffMode's names (tournament = duel, team = tdm);
// - notteam, notfree: out of team modes, out of FFA and Duel;
// - powerups_on, powerups_off: only with powerups on, only with them off
//   (no_powerups, on mm-conven, means powerups_off);
// - ruleset / not_ruleset, notq2: MuffMode's rulesets, the mod counting as
//   its Quake II ones (q2, q2re, mm); the other notq* keep it;
// - bfg_on, bfg_off: the BFG is always allowed, so bfg_off stays out.
// Other known keys (author, notarena...) change nothing.
bool Core_Entities_Excluded(const EntityKeys &keys, const EntitySpawnMode &mode);

// Whether `classname` is in g_disable_items (a list, spaces or commas, any
// case), so it doesn't spawn in deathmatch: MuffMode's disable_<item>
// switches as one setting. Only pickups can be disabled (item_, weapon_,
// ammo_), never a CTF flag: anything else in the list is ignored.
bool Core_Entities_Disabled(std::string_view list, std::string_view classname);

// MuffMode's small and large ammo: stock's ammo with its own amount and
// model, which retail's pak0 carries.
struct AmmoAlias
{
	const char *classname; // "ammo_slugs_small"
	const char *stock;     // "ammo_slugs"
	int count;             // 3
	const char *model;     // "models/vault/items/ammo/slugs/small/tris.md2"
};
const AmmoAlias *Core_Entities_AmmoAlias(std::string_view classname);

using EntityVec = std::array<float, 3>;

// Quake III's jump pad: the velocity that throws a body from `from` (the
// pad's centre) up to peak at `apex` under `gravity`. False when the apex
// isn't above the pad: no throw.
bool Core_Entities_JumpVelocity(const EntityVec &from, const EntityVec &apex, float gravity, EntityVec &velocity);

// A .ent override file fit to replace the BSP's entities: well-formed
// ({ "key" "value" ... } blocks) and starting with worldspawn. Else false,
// with what's wrong in `error`.
bool Core_Entities_ValidFile(std::string_view text, std::string &error);
