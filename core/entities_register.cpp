// Map entities beyond stock, the engine side (docs/design/entities.md): the
// .ent override file, the mode keys kept as an entity is parsed, MuffMode's
// ammo and point entities, the jump pads' aim and `sv saveents`. The rules
// are core/entities.cpp.
#include "g_local.h"

#include "core/entities.h"
#include "core/entities_hooks.h"
#include "core/gametype.h"
#include "core/host_files.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
constexpr size_t kMaxFile = 1 << 20; // a big map's entities are a few hundred KB

std::string level_entities; // what this level spawned from (sv saveents)
const edict_t *keys_owner = nullptr;
EntityKeys keys; // the mode keys of the entity being parsed
bool no_spawnpads = false;
int left_out = 0;

cvar_t *OverrideDir()
{
	return gi.cvar("g_entity_override_dir", "maps", CVAR_NOFLAGS);
}

std::string FileName()
{
	return std::string(level.mapname) + ".ent";
}

// Without the engine's file access, a plain path: <basedir>/<game>/<dir>/<map>.ent,
// from the server's working directory when basedir is unset (MuffMode's).
std::filesystem::path PlainPath()
{
	const char *basedir = gi.cvar("basedir", "", CVAR_NOFLAGS)->string;
	const char *game = gi.cvar("game", "", CVAR_NOFLAGS)->string;
	return std::filesystem::path(basedir) / (*game ? game : "baseq2") / OverrideDir()->string / FileName();
}

std::string GamePath()
{
	return Core_GamePath(OverrideDir()->string, FileName());
}

// The override file's text and where it was found, or false.
bool ReadOverride(std::string &text, std::string &where)
{
	if (Core_HostFiles_Available() && Core_HostFiles_Read(GamePath(), text))
	{
		where = GamePath();
		return true;
	}
	const std::filesystem::path path = PlainPath();
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream data;
	data << in.rdbuf();
	text = data.str();
	where = path.generic_string();
	return true;
}

EntitySpawnMode Mode()
{
	EntitySpawnMode mode;
	const GametypeDef *gametype = Core_Gametype_Active();
	mode.gametype = gametype ? gametype->id : ctf->integer ? "ctf" : teamplay->integer ? "tdm" : "ffa";
	mode.teams = ctf->integer || teamplay->integer;
	mode.powerups = !g_no_items->integer;
	return mode;
}

USE(target_remove_powerups_use)(edict_t *self, edict_t *other, edict_t *activator)->void
{
	if (!activator || !activator->client)
		return;
	gclient_t *cl = activator->client;
	cl->quad_time = cl->double_time = cl->quadfire_time = 0_ms;
	cl->invincible_time = cl->invisible_time = cl->breather_time = cl->enviro_time = 0_ms;
	for (item_id_t id = static_cast<item_id_t>(IT_NULL + 1); id != IT_TOTAL; id = static_cast<item_id_t>(id + 1))
	{
		const gitem_t *item = GetItemByIndex(id);
		if (item && (item->flags & IF_POWERUP))
			cl->pers.inventory[id] = 0;
	}
	CTFDeadDropTech(activator); // as if they had died: back into the level
	CTFDeadDropFlag(activator);
}
} // namespace

const char *Core_Entities_Override(const char *mapname, const char *entities)
{
	keys_owner = nullptr;
	keys.clear();
	no_spawnpads = false;
	left_out = 0;
	level_entities.assign(entities ? entities : "");
	if (!deathmatch->integer || !gi.cvar("g_entity_override_load", "1", CVAR_NOFLAGS)->integer)
		return entities;

	std::string text, where;
	if (!ReadOverride(text, where))
		return entities;
	std::string error;
	if (text.size() > kMaxFile)
		error = "over 1 MB";
	else
		Core_Entities_ValidFile(text, error); // sets `error` when it isn't
	if (!error.empty())
	{
		gi.Com_PrintFmt("Entities: {} not used ({}); the map's own\n", where.c_str(), error.c_str());
		return entities;
	}
	gi.Com_PrintFmt("Entities: {}\n", where.c_str());
	level_entities = std::move(text);
	return level_entities.c_str();
}

bool Core_Entities_Key(edict_t *ent, const char *key, const char *value)
{
	if (!Core_Entities_IsKey(key))
		return false;
	if (ent != keys_owner)
	{
		keys.clear();
		keys_owner = ent;
	}
	keys.emplace_back(key, value);
	if (ent == g_edicts && !Q_strcasecmp(key, "no_dm_spawnpads") && atoi(value))
		no_spawnpads = true;
	return true;
}

bool Core_Entities_Inhibit(edict_t *ent)
{
	EntityKeys mine;
	if (ent == keys_owner)
		mine.swap(keys);
	keys_owner = nullptr;
	keys.clear();
	if (!deathmatch->integer || mine.empty() || !Core_Entities_Excluded(mine, Mode()))
		return false;
	++left_out;
	return true;
}

bool Core_Entities_Spawn(edict_t *ent)
{
	if (!deathmatch->integer)
		return false;
	if (const AmmoAlias *ammo = Core_Entities_AmmoAlias(ent->classname))
	{
		ent->classname = ammo->stock;
		if (!ent->count)
			ent->count = ammo->count;
		ent->model = ammo->model;
		return false;
	}
	if (!Q_strcasecmp(ent->classname, "target_relay")) // Quake III's name for it
	{
		ent->classname = "trigger_relay";
		return false;
	}
	// Points other entities aim at (jump pads) or teleport to.
	if (!Q_strcasecmp(ent->classname, "target_position") || !Q_strcasecmp(ent->classname, "target_teleporter"))
	{
		ent->classname = "info_notnull";
		return false;
	}
	if (!Q_strcasecmp(ent->classname, "target_remove_powerups"))
	{
		ent->use = target_remove_powerups_use;
		ent->svflags = SVF_NOCLIENT;
		return true;
	}
	return false;
}

void Core_Entities_Spawned()
{
	if (!deathmatch->integer)
		return;
	int pads = 0;
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
	{
		edict_t *ent = g_edicts + i;
		if (!ent->inuse || !ent->classname || strcmp(ent->classname, "trigger_push") || !ent->target)
			continue;
		const edict_t *apex = G_PickTarget(ent->target);
		if (!apex)
			continue;
		const vec3_t from = (ent->absmin + ent->absmax) * 0.5f;
		EntityVec velocity{};
		if (!Core_Entities_JumpVelocity({from[0], from[1], from[2]},
		                                {apex->s.origin[0], apex->s.origin[1], apex->s.origin[2]}, sv_gravity->value,
		                                velocity))
			continue;
		// Stock's push: movedir at speed x 10.
		const vec3_t v{velocity[0], velocity[1], velocity[2]};
		const float speed = v.length();
		ent->movedir = v / speed;
		ent->speed = speed / 10;
		++pads;
	}
	if (pads)
		gi.Com_PrintFmt("Entities: {} jump pads aimed\n", pads);
	if (left_out)
		gi.Com_PrintFmt("Entities: {} left out of {} by their keys\n", left_out, Mode().gametype.c_str());
}

bool Core_Entities_NoSpawnPads()
{
	return deathmatch->integer && no_spawnpads;
}

bool Core_Entities_ServerCommand(const char *cmd)
{
	if (Q_strcasecmp(cmd, "saveents"))
		return false;
	if (level_entities.empty())
	{
		gi.Com_Print("No level is loaded.\n");
		return true;
	}
	// Never over an admin's file: they may have edited it since it loaded.
	std::string existing, where;
	if (ReadOverride(existing, where))
	{
		gi.Com_PrintFmt("{} already exists; move it away first.\n", where.c_str());
		return true;
	}
	// Always the plain path: Q2RED's file access writes only records (.json,
	// .txt, .csv, .log), and its search path reads this one back.
	const std::filesystem::path path = PlainPath();
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	std::ofstream out(path, std::ios::binary);
	out << level_entities;
	if (out)
		gi.Com_PrintFmt("Entities written to {}\n", path.generic_string().c_str());
	else
		gi.Com_PrintFmt("Couldn't write {}\n", path.generic_string().c_str());
	return true;
}
