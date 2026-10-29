// Engine-facing part of arriving players (docs/design/joining.md): decides
// where each client arriving goes, remembers who was playing for the next
// level (in g_join_memory, as `map` reloads the module), opens the Tab menu
// for a new arrival, and handles `join` / `spectate`.
#include "g_local.h"

#include "core/gametype.h"
#include "core/join.h"
#include "core/join_hooks.h"
#include "core/menu_hooks.h"
#include "core/player_name.h"
#include "core/teams_hooks.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

// id's, with no declaration in a header.
void spectator_respawn(edict_t *ent);
void CTFJoinTeam(edict_t *ent, ctfteam_t desired_team);

namespace
{
cvar_t *g_auto_join, *g_join_memory;
JoinMemory recalled; // the last level's players, while they come back
bool begun = false;
gtime_t last_time;
std::array<bool, MAX_CLIENTS> plays{};        // Core_Join_Begin's decision
std::array<bool, MAX_CLIENTS> menu_pending{}; // open the menu once they're in

// At a level change every human reconnects, one by one; for this long into
// a level, the last level's players come back as they were (as Duel's queue).
constexpr gtime_t kReturnGrace = 30_sec;

void RegisterCvars()
{
	if (g_auto_join)
		return;
	// 0: a human new to the server arrives spectating, with the menu open.
	g_auto_join = gi.cvar("g_auto_join", "0", CVAR_NOFLAGS);
	// Who was playing (core/join.h): the game's own state, not a setting.
	g_join_memory = gi.cvar("g_join_memory", "", CVAR_NOFLAGS);
}

uint32_t SlotOf(const edict_t *ent)
{
	return (uint32_t)(ent - g_edicts) - 1;
}

// A new level, or the module just loaded: recall what the last level left.
void NewLevel()
{
	RegisterCvars();
	if (begun && level.time >= last_time)
		return;
	begun = true;
	last_time = level.time;
	recalled = JoinMemory::Decode(g_join_memory->string);
	plays.fill(false);
	menu_pending.fill(false);
}

bool Present(const edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->pers.connected;
}

// The humans playing now, or (Duel) waiting to: key and team.
std::vector<std::pair<std::string, int>> Playing()
{
	const GametypeDef *gt = Core_Gametype_Active();
	std::vector<std::pair<std::string, int>> out;
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		const edict_t *ent = g_edicts + i;
		if (!Present(ent) || (ent->svflags & SVF_BOT))
			continue;
		if (G_TeamplayEnabled())
		{
			if (ent->client->resp.ctf_team == CTF_TEAM1 || ent->client->resp.ctf_team == CTF_TEAM2)
				out.emplace_back(Core_PlayerKey(ent), (int)ent->client->resp.ctf_team);
		}
		else if (!ent->client->resp.spectator || (gt && gt->waiting && gt->waiting(ent)))
			out.emplace_back(Core_PlayerKey(ent), 0);
	}
	return out;
}

// The team with fewer players that `ent` may join (ties: either); 0 if both
// are locked to them.
int Smaller(const edict_t *ent)
{
	int count[3] = {};
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		const edict_t *other = g_edicts + i;
		if (other != ent && other->inuse && other->client)
			count[other->client->resp.ctf_team == CTF_TEAM1 ? 1 : other->client->resp.ctf_team == CTF_TEAM2 ? 2 : 0]++;
	}
	edict_t *self = const_cast<edict_t *>(ent);
	const bool red = Core_Teams_MayJoin(self, 1, false), blue = Core_Teams_MayJoin(self, 2, false);
	if (red && blue)
		return count[1] < count[2] ? 1 : count[2] < count[1] ? 2 : brandom() ? 1 : 2;
	return red ? 1 : blue ? 2 : 0;
}
} // namespace

void Core_Join_SetSpectatorInfo(edict_t *ent, bool spectator)
{
	gi.Info_SetValueForKey(ent->client->pers.userinfo, "spectator", spectator ? "1" : "0");
	if (!(ent->svflags & SVF_BOT))
	{
		gi.WriteByte(svc_stufftext);
		gi.WriteString(spectator ? "spectator 1\n" : "spectator 0\n");
		gi.unicast(ent, true);
	}
}

void Core_Join_SetSpectator(edict_t *ent, bool spectator)
{
	Core_Join_SetSpectatorInfo(ent, spectator);
	ent->client->pers.spectator = spectator;
	spectator_respawn(ent);
	// It may refuse (a password, the spectator limit): the key follows.
	if (ent->client->pers.spectator != spectator)
		Core_Join_SetSpectatorInfo(ent, ent->client->pers.spectator);
}

void Core_Join_Begin(edict_t *ent)
{
	if (!deathmatch->integer || !Core_Gametype_Active())
		return;
	NewLevel();
	const uint32_t slot = SlotOf(ent);
	JoinWho who;
	who.bot = (ent->svflags & SVF_BOT) != 0;
	who.host = !sv_dedicated->integer && slot == 0;
	who.remembered = !who.bot && recalled.Recall(Core_PlayerKey(ent), who.team);
	const JoinArrival arrival = Core_Join_Arrive(who, g_auto_join->integer != 0);
	plays[slot] = arrival.play;

	if (G_TeamplayEnabled())
	{
		// Bots: stock's CTFAssignTeam, next. A human on no team gets stock's
		// join menu, which is ours (Core_Menu_JoinMenu).
		if (who.bot)
			return;
		ent->client->resp.ctf_team = CTF_NOTEAM;
		if (arrival.play)
		{
			const int team = arrival.team == 1 || arrival.team == 2 ? arrival.team : Smaller(ent);
			if (team)
				ent->client->resp.ctf_team = (ctfteam_t)team;
		}
		return;
	}
	// FFA; Duel's ClientBegin, next, decides from its queue.
	Core_Join_SetSpectatorInfo(ent, !arrival.play);
	ent->client->pers.spectator = !arrival.play;
	menu_pending[slot] = !arrival.play && !who.bot;
}

bool Core_Join_Plays(const edict_t *ent)
{
	return plays[SlotOf(ent)];
}

void Core_Join_Frame()
{
	if (!deathmatch->integer || !Core_Gametype_Active())
		return;
	NewLevel();
	last_time = level.time;

	// Who is playing, for the next level. In the grace the last level's
	// players not back yet are kept too (a map change straight away mustn't
	// lose them); after it the last level is forgotten, and a player
	// reconnecting mid-level arrives as anyone new.
	JoinMemory now;
	now.Set(Playing());
	if (level.time < kReturnGrace)
	{
		std::vector<std::string> present;
		for (uint32_t i = 1; i <= game.maxclients; ++i)
		{
			if (Present(g_edicts + i))
				present.push_back(Core_PlayerKey(g_edicts + i));
		}
		now.KeepAbsent(recalled, present);
	}
	else if (!recalled.Empty())
		recalled = JoinMemory();
	const std::string text = now.Encode();
	if (text != g_join_memory->string)
		gi.cvar_set("g_join_memory", text.c_str());

	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = g_edicts + 1 + i;
		if (!menu_pending[i] || !Present(ent) || !ent->client->pers.spawned)
			continue;
		menu_pending[i] = false;
		if (ent->client->resp.spectator && !ent->client->menu && !level.intermissiontime)
			Core_Menu_Inven(ent);
	}
}

void Core_Join_Run(edict_t *ent, bool join)
{
	// The gametype's own first: Duel's queue.
	if (const GametypeDef *gt = Core_Gametype_Active();
	    gt && gt->client_command && gt->client_command(ent, join ? "join" : "spectate"))
		return;

	if (G_TeamplayEnabled())
	{
		const bool on_team = ent->client->resp.ctf_team == CTF_TEAM1 || ent->client->resp.ctf_team == CTF_TEAM2;
		if (join && on_team)
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_already_on_team", CTFTeamName(ent->client->resp.ctf_team));
		else if (join)
		{
			const int team = Smaller(ent);
			if (team)
				CTFJoinTeam(ent, (ctfteam_t)team);
			else
				gi.LocClient_Print(ent, PRINT_HIGH, "Both teams are locked.\n");
		}
		else if (on_team)
			CTFObserver(ent); // its respawn reopens the join menu
		return;
	}

	if (join == !ent->client->resp.spectator)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, join ? "You're already playing.\n" : "You're already spectating.\n");
		return;
	}
	Core_Join_SetSpectator(ent, !join);
}

bool Core_Join_ClientCommand(edict_t *ent, const char *cmd)
{
	const bool join = !Q_strcasecmp(cmd, "join");
	if (!join && Q_strcasecmp(cmd, "spectate"))
		return false;
	if (!deathmatch->integer || !Core_Gametype_Active())
		return false;
	Core_Join_Run(ent, join);
	return true;
}
