// Engine-facing part of Duel: keeps the two queue-front clients playing and
// everyone else spectating, forfeits a match a duellist leaves, and rotates the
// loser to the back of the queue at the end.
#include "g_local.h"

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

std::vector<int> Present()
{
	std::vector<int> ids;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = Slot((int)i);
		if (ent->inuse && ent->client && ent->client->pers.connected && ent->client->pers.spawned)
			ids.push_back((int)i);
	}
	return ids;
}

bool IsPlaying(const edict_t *ent)
{
	return !ent->client->resp.spectator;
}

// Moves a client in or out of the game, bypassing spectator_respawn's
// password and maxspectators checks: the queue, not the player, decides.
void SetPlaying(edict_t *ent, bool playing)
{
	ent->client->pers.spectator = !playing;
	ent->client->resp.spectator = !playing;
	ent->client->resp.score = ent->client->pers.score = 0;
	ent->svflags &= ~SVF_NOCLIENT;
	PutClientInServer(ent);
	if (playing)
		gi.LocBroadcast_Print(PRINT_HIGH, "{} steps up to duel.\n", ent->client->pers.netname);
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "You're in the duel queue.\n");
}
} // namespace

// DMGame.ClientBegin: a newcomer who isn't in the front two spawns straight in
// as a spectator, rather than appearing in the arena for a frame.
static void Duel_ClientBegin(edict_t *ent)
{
	queue.Sync(Present());
	const int slot = (int)(ent - g_edicts) - 1;
	const bool playing = queue.IsPlayer(slot);
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
	std::string text;
	int place = 0;
	for (int id : queue.Order())
	{
		const char *role = place < 2 ? "duelling" : "waiting";
		text += G_Fmt("{}. {} ({})\n", place + 1, Slot(id)->client->pers.netname, role).data();
		++place;
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "{}", text.empty() ? "The duel queue is empty.\n" : text.c_str());
	return true;
}

void Duel_Install(dm_game_rt &hooks)
{
	hooks.ClientBegin = Duel_ClientBegin;
}

void Duel_Frame()
{
	queue.Sync(Present());
	if (level.intermissiontime)
		return;
	for (int id : Present())
	{
		edict_t *ent = Slot(id);
		const bool should = queue.IsPlayer(id);
		if (IsPlaying(ent) != should)
			SetPlaying(ent, should);
	}
}

bool Duel_MatchOver()
{
	// A duellist left mid-match: it's a forfeit.
	int playing = 0;
	for (int id : Present())
		playing += IsPlaying(Slot(id));
	return playing < 2;
}

void Duel_MatchEnded()
{
	const std::vector<int> players = queue.Players();
	if (players.size() < 2)
		return;
	edict_t *a = Slot(players[0]), *b = Slot(players[1]);
	// The lower score loses; on a tie the challenger (second in line) does,
	// so the champion keeps the spot.
	const int loser = a->client->resp.score < b->client->resp.score ? players[0] : players[1];
	queue.Rotate(loser);
	gi.Com_PrintFmt("Duel: {} {} - {} {}; next: {}\n", a->client->pers.netname, a->client->resp.score,
	                b->client->resp.score, b->client->pers.netname, Slot(loser)->client->pers.netname);
}
