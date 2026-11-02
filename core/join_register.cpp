// Engine-facing part of arriving players (docs/design/joining.md): decides
// where each client arriving goes, remembers who was playing for the next
// level (in g_join_memory, as `map` reloads the module), opens the Tab menu
// for a new arrival, and handles `join` / `spectate`.
#include "g_local.h"

#include "core/gametype.h"
#include "core/join.h"
#include "core/join_hooks.h"
#include "core/match_hooks.h"
#include "core/menu_hooks.h"
#include "core/player_name.h"
#include "core/teams.h"
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
// FFA and Duel: each client's spectating as the server decided it, by key
// (core/player_name.h). The KEX client keeps no `spectator` userinfo, so
// every userinfo it sends would otherwise clear the key (and, five seconds
// later, put a spectator into play).
std::array<std::pair<std::string, bool>, MAX_CLIENTS> decided{};

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
	g_join_memory = gi.cvar("g_join_memory", "", CVAR_NOSET); // internal: only the module sets it
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
	// The server's copy only: a stuffed `spectator 1` isn't a setting on the
	// KEX client, which sends it back as an unknown command (playtest
	// 2026-10-29: "invalid game command"). Core_Join_UserinfoChanged keeps it.
	gi.Info_SetValueForKey(ent->client->pers.userinfo, "spectator", spectator ? "1" : "0");
	decided[SlotOf(ent)] = {Core_PlayerKey(ent), spectator};
}

void Core_Join_UserinfoChanged(edict_t *ent)
{
	const auto &[key, spectator] = decided[SlotOf(ent)];
	if (key.empty() || key != Core_PlayerKey(ent))
		return;
	gi.Info_SetValueForKey(ent->client->pers.userinfo, "spectator", spectator ? "1" : "0");
	ent->client->pers.spectator = spectator;
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
		gi.cvar_forceset("g_join_memory", text.c_str());

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
	// Stock's spectator_respawn stuffs `spectator 0` / `1`, which the KEX
	// client sends straight back as a command: nothing to do, nothing to say.
	if (!Q_strcasecmp(cmd, "spectator"))
		return true;
	const bool join = !Q_strcasecmp(cmd, "join");
	if (!join && Q_strcasecmp(cmd, "spectate"))
		return false;
	if (!deathmatch->integer || !Core_Gametype_Active())
		return false;
	Core_Join_Run(ent, join);
	return true;
}

// `sv setteam <player> red|blue`: the server puts a player on a team, as if
// they had joined it themselves: not past a lock.
static bool SetTeamCommand()
{
	const char *usage = "Usage: sv setteam <player> red|blue (name or client number; team games only)\n";
	const char *word = gi.argc() >= 4 ? gi.argv(gi.argc() - 1) : "";
	const int team = !Q_strcasecmp(word, "red") ? CTF_TEAM1 : !Q_strcasecmp(word, "blue") ? CTF_TEAM2 : CTF_NOTEAM;
	if (!deathmatch->integer || !G_TeamplayEnabled() || team == CTF_NOTEAM)
	{
		gi.Com_Print(usage);
		return true;
	}
	// The words between the command and the team: a name may have spaces.
	std::string arg = gi.argv(2);
	if (gi.argc() > 4)
	{
		std::string rest = gi.args() + strlen(gi.argv(1));
		rest.erase(0, rest.find_first_not_of(' '));
		rest.erase(rest.find_last_not_of(' ') + 1);
		arg = rest.substr(0, rest.size() - strlen(word));
		arg.erase(arg.find_last_not_of(' ') + 1);
	}
	edict_t *ent = Core_PlayerNamed(arg.c_str());
	if (!ent)
	{
		gi.Com_PrintFmt("No one player called '{}'.\n", arg);
		return true;
	}
	if (ent->svflags & SVF_BOT)
	{
		gi.Com_Print("Bots are the server's to place: use bot_count.\n");
		return true;
	}
	const std::string name = Core_PlayerName(ent);
	if (ent->client->resp.ctf_team == team)
	{
		gi.Com_PrintFmt("{} is already on {}.\n", name, CTFTeamName(team));
		return true;
	}
	if (!Core_Teams_MayJoin(ent, team, false))
	{
		gi.Com_PrintFmt("{} is locked: unlock it first (unlockteam {}).\n", CTFTeamName(team), word);
		return true;
	}
	CTFJoinTeam(ent, (ctfteam_t)team);
	if (ent->client->resp.ctf_team != team)
	{
		gi.Com_PrintFmt("{} couldn't be put on {}.\n", name, CTFTeamName(team));
		return true;
	}
	gi.LocCenter_Print(ent, "The server put you on {}.", CTFTeamName(team));
	gi.Com_PrintFmt("Join: {} put on {} by the server\n", name, CTFTeamName(team));
	return true;
}

bool Core_Join_ServerCommand(const char *cmd)
{
	if (!Q_strcasecmp(cmd, "setteam"))
		return SetTeamCommand();
	if (Q_strcasecmp(cmd, "spectate"))
		return false;
	if (!deathmatch->integer || !Core_Gametype_Active() || gi.argc() < 3)
	{
		gi.Com_Print("Usage: sv spectate <player> (name or client number; deathmatch only)\n");
		return true;
	}
	// One argument (quotes and all), or the rest of the line: a name may have spaces.
	const char *arg = gi.argv(2);
	if (gi.argc() > 3)
	{
		arg = gi.args() + strlen(gi.argv(1));
		while (*arg == ' ')
			++arg;
	}
	edict_t *ent = Core_PlayerNamed(arg);
	if (!ent)
	{
		gi.Com_PrintFmt("No one player called '{}'.\n", arg);
		return true;
	}
	if (ent->svflags & SVF_BOT)
	{
		gi.Com_Print("Bots are the server's to place: use bot_count.\n");
		return true;
	}
	const GametypeDef *gt = Core_Gametype_Active();
	const bool queued = gt->waiting && gt->waiting(ent);
	if (!Core_Match_Playing(ent) && !queued)
	{
		gi.Com_PrintFmt("{} is already spectating.\n", Core_PlayerName(ent).c_str());
		return true;
	}
	const std::string name = Core_PlayerName(ent);
	if (G_TeamplayEnabled())
	{
		// Off the team's roster too, as a captain's teamkick: a locked team keeps them out.
		if (g_teamplay_force_join->integer)
		{
			gi.Com_Print("g_teamplay_force_join puts everyone on a team: there's nowhere to move them to.\n");
			return true;
		}
		Core_Teams_Remove(ent);
	}
	else
	{
		// Duel (a queue): a duellist taken out of a match under way isn't a
		// forfeit; the match goes back to warmup and the next in line steps up
		// (Chris, 2026-10-31).
		if (queued && Core_Match_Playing(ent) && Core_Match_InPlay())
			Core_Match_BackToWarmup("a player removed");
		Core_Join_Run(ent, false);
		if (Core_Match_Playing(ent))
		{
			gi.Com_PrintFmt("{} couldn't be moved to spectate (the spectator limit?).\n", name.c_str());
			return true;
		}
		gi.LocCenter_Print(ent, "The server moved you to spectate.\nTab opens the menu.");
	}
	gi.LocBroadcast_Print(PRINT_HIGH, "{} was moved to spectate.\n", ent->client->pers.netname);
	gi.Com_PrintFmt("Join: {} moved to spectate by the server\n", name.c_str());
	return true;
}
