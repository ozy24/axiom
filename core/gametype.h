// The gametype registry (docs/design/gametype-api.md).
//
// A gametype is plain data: an id, a title and an optional install function
// that fills the shared dm_game_rt DMGame table. The enabled gametypes are
// listed in CMake (GAME_GAMETYPES), which generates the table below. Rogue's
// stock modes (Tag, DeathBall) are not gametypes here: the stock gamerules
// cvar still runs them exactly as stock (decision 0010).
#pragma once

#include <cstddef>
#include <string_view>

struct dm_game_rt;

struct GametypeDef
{
	const char *id;                     // g_gametype value, lowercase: "ffa"
	const char *title;                  // shown to players: "Free For All"
	void (*install)(dm_game_rt &hooks); // may be null: stock behaviour
};

// The generated registry (build/generated/gametype_registry.cpp).
extern const GametypeDef *const core_gametypes[];
extern const size_t core_gametype_count;

// Lookup over a table, so tests can pass their own. nullptr if not found.
const GametypeDef *Core_Gametype_Find(const GametypeDef *const *table, size_t count, std::string_view id);

// Selects the gametype from g_gametype and fills DMGame. Called from InitGame
// where Rogue called InitGameRules. With the stock gamerules cvar set, or
// outside deathmatch, it does exactly what stock did.
void Core_Gametype_Start();

// The gametype selected by Core_Gametype_Start, or nullptr while Rogue's stock
// gamerules modes run (and before it runs).
const GametypeDef *Core_Gametype_Active();
