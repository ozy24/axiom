// Engine-facing part of the column scoreboard (docs/design/scoreboard.md):
// counts deaths and time played, and builds the board for each viewer. The
// rules are core/scoreboard.cpp.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match_hooks.h"
#include "core/player_name.h"
#include "core/scoreboard.h"
#include "core/scoreboard_hooks.h"

#include <array>
#include <ctime>

namespace
{
cvar_t *g_scoreboard;

// Stock's limit is 1024 bytes with its terminator. After ours come its
// frag limit, clock and intermission line, each when it shows: room for them.
size_t Budget(size_t used)
{
	size_t reserve = 1 + 40; // the terminator; the clock
	if (fraglimit->integer)
		reserve += 56;
	if (level.intermissiontime)
		reserve += 80;
	return used + reserve < 1024 ? 1024 - reserve - used : 0;
}

struct Count
{
	std::string key; // whose (core/player_name.h): a new client starts afresh
	int deaths = 0;
	int64_t played_ms = 0;
};
std::array<Count, MAX_CLIENTS> counts;
bool was_in_play = false;
bool begun = false;
gtime_t last_time;

void RegisterCvars()
{
	if (g_scoreboard)
		return;
	// 0: stock's board; 1: the column board (outside team modes).
	g_scoreboard = gi.cvar("g_scoreboard", "0", CVAR_NOFLAGS);
}

// Deathmatch outside team modes, under a registered gametype (single player,
// coop and Rogue's stock gamerules modes stay stock).
bool Active()
{
	return deathmatch->integer && Core_Gametype_Active() && !G_TeamplayEnabled();
}

edict_t *ClientEdict(uint32_t i)
{
	return g_edicts + 1 + i;
}

bool Present(const edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->pers.connected;
}

// The slot's count, started afresh if the client there changed.
Count &CountOf(uint32_t i)
{
	Count &c = counts[i];
	const std::string key = Core_PlayerKey(ClientEdict(i));
	if (c.key != key)
		c = Count{key};
	return c;
}

// The header's server time: "2026-10-29 19:40".
std::string Stamp()
{
	const std::time_t now = std::time(nullptr);
	std::tm local{};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char text[32];
	std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &local);
	return text;
}
} // namespace

void Core_Scoreboard_Frame()
{
	if (!Active())
		return;
	RegisterCvars();
	if (!begun || level.time < last_time)
	{
		counts.fill(Count{});
		begun = true;
		was_in_play = false;
		last_time = level.time;
	}
	const int64_t step = (level.time - last_time).milliseconds();
	last_time = level.time;

	// The match starting: everyone from nothing (stock's scores are reset too).
	const bool in_play = Core_Match_InPlay();
	if (in_play && !was_in_play)
	{
		for (Count &c : counts)
			c.deaths = 0, c.played_ms = 0;
	}
	was_in_play = in_play;
	if (!in_play || level.intermissiontime)
		return;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = ClientEdict(i);
		if (Present(ent) && Core_Match_Playing(ent))
			CountOf(i).played_ms += step;
	}
}

void Core_Scoreboard_Died(const edict_t *self)
{
	if (!Active() || !self->client || !Core_Match_InPlay() || level.intermissiontime)
		return;
	CountOf((uint32_t)(self - g_edicts) - 1).deaths++;
}

bool Core_Scoreboard_Columns(const edict_t *ent, std::string &layout)
{
	if (!Active())
		return false;
	RegisterCvars();
	if (!g_scoreboard->integer)
		return false;
	ScoreboardInput in;
	in.viewer = (int)(ent - g_edicts) - 1;
	in.stamp = Stamp();
	in.budget = Budget(layout.size());
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *other = ClientEdict(i);
		if (!Present(other))
			continue;
		const Count &c = CountOf(i);
		ScoreboardPlayer p;
		p.slot = (int)i;
		p.frags = other->client->resp.score;
		p.deaths = c.deaths;
		p.ping = other->client->ping;
		p.played_ms = c.played_ms;
		p.spectator = !Core_Match_Playing(other);
		in.players.push_back(p);
	}
	layout += Core_Scoreboard_Layout(in);
	return true;
}
