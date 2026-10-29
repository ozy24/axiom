// Engine-facing part of Duel: keeps the two queue-front clients playing and
// everyone else spectating, forfeits a match a duellist leaves, and rotates the
// loser to the back of the queue at the end.
#include "g_local.h"

#include "core/player_name.h"
#include "gametypes/duel/duel_queue.h"

#include <string>
#include <vector>

namespace
{
DuelQueue queue;

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

std::vector<std::string> Present()
{
	std::vector<std::string> keys;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = Slot((int)i);
		if (IsPresent(ent))
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
	queue.Sync(Present(), level.time < kReturnGrace);
}

bool IsPlaying(const edict_t *ent)
{
	return !ent->client->resp.spectator;
}

// PutClientInServer re-reads the "spectator" userinfo key (via
// ClientUserinfoChanged), so the queue's decision has to be written there or
// the next spawn undoes it; players also get their own cvar set, as
// spectator_respawn does.
void SetSpectatorInfo(edict_t *ent, bool spectator)
{
	gi.Info_SetValueForKey(ent->client->pers.userinfo, "spectator", spectator ? "1" : "0");
	if (!(ent->svflags & SVF_BOT))
	{
		gi.WriteByte(svc_stufftext);
		gi.WriteString(spectator ? "spectator 1\n" : "spectator 0\n");
		gi.unicast(ent, true);
	}
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
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "{} waits in the duel queue.\n", ent->client->pers.netname);
}
} // namespace

// DMGame.ClientBegin: a newcomer who isn't in the front two spawns straight in
// as a spectator, rather than appearing in the arena for a frame.
static void Duel_ClientBegin(edict_t *ent)
{
	Sync();
	const bool playing = queue.IsPlayer(Key(ent));
	SetSpectatorInfo(ent, !playing);
	ent->client->pers.spectator = !playing;
	ent->client->resp.spectator = !playing;
	if (!playing)
		gi.LocBroadcast_Print(PRINT_HIGH, "{} waits in the duel queue.\n", ent->client->pers.netname);
}

// `queue`: who's duelling and who's next.
bool Duel_ClientCommand(edict_t *ent, const char *cmd)
{
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
