// Engine-facing part of Duel: keeps the two queue-front clients playing and
// everyone else spectating, forfeits a match a duellist leaves, and rotates the
// loser to the back of the queue at the end.
#include "g_local.h"

#include "core/join_hooks.h"
#include "core/player_name.h"
#include "gametypes/duel/duel_queue.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

void Duel_Frame();

namespace
{
DuelQueue queue;
// Who has asked to play (`join`, the menu's Join Queue; bots, and the last
// level's players, on arrival: core/join.h). Only they are queued.
std::unordered_set<std::string> wants;

edict_t *Slot(int i)
{
	return g_edicts + 1 + i;
}

bool IsPresent(const edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->pers.connected && ent->client->pers.spawned;
}

// The queue's key for a client (core/player_name.h).
std::string Key(const edict_t *ent)
{
	return Core_PlayerKey(ent);
}

// The clients present who want to play.
std::vector<std::string> Present()
{
	std::vector<std::string> keys;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = Slot((int)i);
		if (IsPresent(ent) && wants.count(Key(ent)))
			keys.push_back(Key(ent));
	}
	return keys;
}

// The client a key belongs to: the present one if any (a human may have come
// back in a new slot), else the slot that last held it, for its name.
edict_t *Find(const std::string &key)
{
	edict_t *last = nullptr;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = Slot((int)i);
		if (!ent->client || Key(ent) != key)
			continue;
		if (IsPresent(ent))
			return ent;
		last = ent;
	}
	return last;
}

// At a level change every client is marked gone and re-enters one by one (a
// human as a new connection, which can take seconds); for this long into a
// level, absent clients keep their place in the queue.
constexpr gtime_t kReturnGrace = 30_sec;

void Sync()
{
	const bool grace = level.time < kReturnGrace;
	queue.Sync(Present(), grace);
	// After the grace, someone gone is gone: back later, they ask again.
	if (!grace)
	{
		for (auto it = wants.begin(); it != wants.end();)
			it = std::find(queue.Order().begin(), queue.Order().end(), *it) == queue.Order().end() ? wants.erase(it)
			                                                                                       : std::next(it);
	}
}

bool IsPlaying(const edict_t *ent)
{
	return !ent->client->resp.spectator;
}

// PutClientInServer re-reads the "spectator" userinfo key (via
// ClientUserinfoChanged), so the queue's decision has to be written there or
// the next spawn undoes it (core/join_hooks.h).
void SetSpectatorInfo(edict_t *ent, bool spectator)
{
	Core_Join_SetSpectatorInfo(ent, spectator);
}

// Moves a client in or out of the game, bypassing spectator_respawn's
// password and maxspectators checks: the queue, not the player, decides.
void SetPlaying(edict_t *ent, bool playing)
{
	SetSpectatorInfo(ent, !playing);
	ent->client->pers.spectator = !playing;
	ent->client->resp.spectator = !playing;
	ent->client->resp.score = ent->client->pers.score = 0;
	ent->svflags &= ~SVF_NOCLIENT;
	PutClientInServer(ent);
	if (playing)
		gi.LocBroadcast_Print(PRINT_HIGH, "{} steps up to duel.\n", ent->client->pers.netname);
	else if (wants.count(Key(ent)))
		gi.LocBroadcast_Print(PRINT_HIGH, "{} waits in the duel queue.\n", ent->client->pers.netname);
}
} // namespace

// DMGame.ClientBegin: a bot, the last level's players and (g_auto_join) any
// newcomer join the queue (core/join.h); anyone else arrives spectating, with
// the menu's Join Queue. A newcomer who isn't in the front two spawns straight
// in as a spectator, rather than appearing in the arena for a frame.
static void Duel_ClientBegin(edict_t *ent)
{
	if (Core_Join_Plays(ent))
		wants.insert(Key(ent));
	Sync();
	const bool playing = queue.IsPlayer(Key(ent));
	SetSpectatorInfo(ent, !playing);
	ent->client->pers.spectator = !playing;
	ent->client->resp.spectator = !playing;
	if (!playing && wants.count(Key(ent)))
		gi.LocBroadcast_Print(PRINT_HIGH, "{} waits in the duel queue.\n", ent->client->pers.netname);
}

bool Duel_Waiting(const edict_t *ent)
{
	return wants.count(Key(ent)) != 0;
}

// `join` / `spectate`: into the queue or out of it (out of a duel is a
// forfeit); Duel_Frame moves them in or out of play.
static bool JoinCommand(edict_t *ent, bool join)
{
	const std::string key = Key(ent);
	if (join == (wants.count(key) != 0))
	{
		gi.LocClient_Print(ent, PRINT_HIGH,
		                   join ? "You're already in the duel queue.\n" : "You're not in the duel queue.\n");
		return true;
	}
	if (join)
	{
		wants.insert(key);
		gi.LocBroadcast_Print(PRINT_HIGH, "{} joins the duel queue.\n", ent->client->pers.netname);
	}
	else
	{
		wants.erase(key);
		queue.Leave(key);
		gi.LocBroadcast_Print(PRINT_HIGH, "{} leaves the duel queue.\n", ent->client->pers.netname);
	}
	Duel_Frame();
	return true;
}

// `queue`: who's duelling and who's next; `join` / `spectate`.
bool Duel_ClientCommand(edict_t *ent, const char *cmd)
{
	if (!Q_strcasecmp(cmd, "join") || !Q_strcasecmp(cmd, "spectate"))
		return JoinCommand(ent, !Q_strcasecmp(cmd, "join"));
	if (Q_strcasecmp(cmd, "queue"))
		return false;
	// One line each, with the name as its own argument: the client decodes a
	// lobby player's name token only then (core/player_name.h).
	int place = 0;
	for (const std::string &key : queue.Order())
	{
		const edict_t *who = Find(key);
		const bool here = who && IsPresent(who);
		const char *role = !here ? "rejoining" : place < 2 ? "duelling" : "waiting";
		gi.LocClient_Print(ent, PRINT_HIGH, "{}. {} ({})\n", G_Fmt("{}", place + 1).data(),
		                   who ? who->client->pers.netname : "?", role);
		++place;
	}
	if (!place)
		gi.LocClient_Print(ent, PRINT_HIGH, "The duel queue is empty.\n");
	return true;
}

void Duel_Install(dm_game_rt &hooks)
{
	hooks.ClientBegin = Duel_ClientBegin;
}

void Duel_Frame()
{
	Sync();
	if (level.intermissiontime)
		return;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = Slot((int)i);
		if (!IsPresent(ent))
			continue;
		const bool should = queue.IsPlayer(Key(ent));
		if (IsPlaying(ent) != should)
			SetPlaying(ent, should);
	}
}

const char *Duel_MatchOver()
{
	// A duellist left mid-match: it's a forfeit.
	int playing = 0;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = Slot((int)i);
		playing += IsPresent(ent) && IsPlaying(ent);
	}
	return playing < 2 ? "forfeit" : nullptr;
}

void Duel_MatchEnded()
{
	const std::vector<std::string> players = queue.Players();
	if (players.size() < 2)
		return;
	edict_t *a = Find(players[0]), *b = Find(players[1]);
	if (!a || !b)
		return;
	// The lower score loses; on a tie the challenger (second in line) does,
	// so the champion keeps the spot.
	const bool first_lost = a->client->resp.score < b->client->resp.score;
	queue.Rotate(first_lost ? players[0] : players[1]);
	gi.Com_PrintFmt("Duel: {} {} - {} {}; next: {}\n", Core_PlayerName(a).c_str(), a->client->resp.score,
	                b->client->resp.score, Core_PlayerName(b).c_str(), Core_PlayerName(first_lost ? a : b).c_str());
}
