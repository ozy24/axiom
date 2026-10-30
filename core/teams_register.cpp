// Engine-facing part of team locks and captains (docs/design/teams.md): feeds
// core/teams.cpp the clients each frame, announces what it decides, gates
// stock's team joins, and handles the captains' and admins' commands.
#include "g_local.h"

#include "core/match_hooks.h"
#include "core/menu.h"
#include "core/player_name.h"
#include "core/teams.h"
#include "core/teams_hooks.h"

#include <cctype>
#include <string>
#include <vector>

namespace
{
Teams teams;
bool begun = false;
gtime_t last_time;
bool was_in_play = false; // the match flow was live (or between rounds) last frame
bool was_warmup = false;
cvar_t *match_lock;

bool Active()
{
	return deathmatch->integer && G_TeamplayEnabled();
}

edict_t *Slot(uint32_t i)
{
	return g_edicts + 1 + i;
}

bool Present(const edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->pers.connected;
}

int TeamOf(const edict_t *ent)
{
	return ent->client->resp.ctf_team == CTF_TEAM1 ? 1 : ent->client->resp.ctf_team == CTF_TEAM2 ? 2 : 0;
}

const char *TeamName(int team)
{
	return CTFTeamName(team == 1 ? CTF_TEAM1 : CTF_TEAM2);
}

// The client present with `key`, or null.
edict_t *Find(const std::string &key)
{
	if (key.empty())
		return nullptr;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = Slot(i);
		if (Present(ent) && Core_PlayerKey(ent) == key)
			return ent;
	}
	return nullptr;
}

std::vector<TeamsPlayer> Snapshot()
{
	std::vector<TeamsPlayer> players;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = Slot(i);
		if (Present(ent))
			players.push_back({Core_PlayerKey(ent), TeamOf(ent), (ent->svflags & SVF_BOT) != 0});
	}
	return players;
}

void Announce(const std::vector<TeamsEvent> &events)
{
	for (const TeamsEvent &e : events)
	{
		switch (e.kind)
		{
		case TeamsEvent::Kind::Captain:
			if (const edict_t *ent = Find(e.key))
				gi.LocBroadcast_Print(PRINT_HIGH, "{} is captain of {}.\n", ent->client->pers.netname,
				                      TeamName(e.team));
			break;
		case TeamsEvent::Kind::Locked:
			gi.LocBroadcast_Print(PRINT_HIGH, "{} is locked.\n", TeamName(e.team));
			break;
		case TeamsEvent::Kind::Unlocked:
			gi.LocBroadcast_Print(PRINT_HIGH, "{} is unlocked: no players left on it.\n", TeamName(e.team));
			break;
		}
		gi.Com_PrintFmt("Teams: {} {}{}\n", TeamName(e.team),
		                e.kind == TeamsEvent::Kind::Captain  ? "captain "
		                : e.kind == TeamsEvent::Kind::Locked ? "locked"
		                                                     : "unlocked (empty)",
		                e.kind == TeamsEvent::Kind::Captain ? Core_PlayerName(Find(e.key)) : std::string());
	}
}

// Why a captain's command was refused, to them.
void Refused(edict_t *ent, TeamsResult r, int team)
{
	switch (r)
	{
	case TeamsResult::Ok:
		break;
	case TeamsResult::NoTeam:
		gi.LocClient_Print(ent, PRINT_HIGH, "Join a team first.\n");
		break;
	case TeamsResult::NotCaptain:
		gi.LocClient_Print(ent, PRINT_HIGH, "Only your team's captain can do that.\n");
		break;
	case TeamsResult::NotTeammate:
		gi.LocClient_Print(ent, PRINT_HIGH, "They're not on your team.\n");
		break;
	case TeamsResult::Bot:
		gi.LocClient_Print(ent, PRINT_HIGH, "Bots are the server's to place.\n");
		break;
	case TeamsResult::Self:
		gi.LocClient_Print(ent, PRINT_HIGH, "Not on yourself.\n");
		break;
	case TeamsResult::Already:
		gi.LocClient_Print(ent, PRINT_HIGH, teams.Locked(team) ? "{} is already locked.\n" : "{} isn't locked.\n",
		                   TeamName(team));
		break;
	case TeamsResult::Empty:
		gi.LocClient_Print(ent, PRINT_HIGH, "{} has nobody on it.\n", TeamName(team));
		break;
	}
}

// The client a command's argument names: a name (as typed, any case) or a
// client number. Null if none or more than one.
edict_t *Named(const char *arg)
{
	edict_t *found = nullptr;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = Slot(i);
		if (Present(ent) && !Q_strcasecmp(Core_PlayerName(ent).c_str(), arg))
		{
			if (found)
				return nullptr;
			found = ent;
		}
	}
	if (found)
		return found;
	bool digits = *arg != '\0';
	for (const char *c = arg; *c; ++c)
		digits = digits && isdigit((unsigned char)*c);
	if (digits)
	{
		const uint32_t n = (uint32_t)atoi(arg);
		if (n < game.maxclients && Present(Slot(n)))
			return Slot(n);
	}
	return nullptr;
}

void Captain(edict_t *ent, edict_t *target)
{
	const int team = TeamOf(ent);
	if (!target)
	{
		if (!team)
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "Join a team first.\n");
			return;
		}
		const edict_t *captain = Find(teams.Captain(team));
		if (captain == ent)
			gi.LocClient_Print(ent, PRINT_HIGH, "You are captain of {}.\n", TeamName(team));
		else if (captain)
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is captain of {}.\n", captain->client->pers.netname,
			                   TeamName(team));
		return;
	}
	const TeamsResult r = teams.Transfer(Core_PlayerKey(ent), Core_PlayerKey(target));
	if (r != TeamsResult::Ok)
		return Refused(ent, r, team);
	gi.LocBroadcast_Print(PRINT_HIGH, "{} is captain of {}.\n", target->client->pers.netname, TeamName(team));
	gi.Com_PrintFmt("Teams: {} captain {}\n", TeamName(team), Core_PlayerName(target).c_str());
}

void LockTeam(edict_t *ent, bool lock)
{
	const int team = TeamOf(ent);
	const TeamsResult r = teams.CaptainLock(Core_PlayerKey(ent), lock);
	if (r != TeamsResult::Ok)
		return Refused(ent, r, team);
	gi.LocBroadcast_Print(PRINT_HIGH, lock ? "{} is locked.\n" : "{} is unlocked.\n", TeamName(team));
	gi.Com_PrintFmt("Teams: {} {} by {}\n", TeamName(team), lock ? "locked" : "unlocked", Core_PlayerName(ent).c_str());
}

void Kick(edict_t *ent, edict_t *target)
{
	const int team = TeamOf(ent);
	// Force-join puts everyone on a team: there's nowhere to kick them to.
	if (g_teamplay_force_join->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "The server assigns teams here.\n");
		return;
	}
	const TeamsResult r = teams.Kick(Core_PlayerKey(ent), Core_PlayerKey(target));
	if (r != TeamsResult::Ok)
		return Refused(ent, r, team);
	gi.LocBroadcast_Print(PRINT_HIGH, "{} was removed from {}.\n", target->client->pers.netname, TeamName(team));
	gi.Com_PrintFmt("Teams: {} kicked from {} by {}\n", Core_PlayerName(target).c_str(), TeamName(team),
	                Core_PlayerName(ent).c_str());
	CTFObserver(target); // drops a carried flag and tech
	// Not stock's join menu, which the respawn opens: its cursor sits on a
	// team, and a player kicked mid-fight joined it with their next jump
	// (playtest 2026-10-30: kicked to the other team).
	PMenu_Close(target);
	gi.LocCenter_Print(target, "You were removed from {}.\nTab opens the menu.", TeamName(team));
	Announce(teams.Sync(Snapshot()));
}

void ReadyTeam(edict_t *ent)
{
	const int team = TeamOf(ent);
	std::vector<std::string> keys;
	const TeamsResult r = teams.ReadyTeam(Core_PlayerKey(ent), keys);
	if (r != TeamsResult::Ok)
		return Refused(ent, r, team);
	if (!Core_Match_ReadyUp())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "There is no ready-up now.\n");
		return;
	}
	for (const std::string &key : keys)
	{
		if (edict_t *mate = Find(key))
			Core_Match_SetReady(mate, true, false);
	}
	gi.LocBroadcast_Print(PRINT_HIGH, "{} readied {}.\n", ent->client->pers.netname, TeamName(team));
}

// `sv lockteam red|blue` and friends: the team named in argv(2), or 0.
int ArgTeam()
{
	const char *t = gi.argv(2);
	return !Q_strcasecmp(t, "red") ? 1 : !Q_strcasecmp(t, "blue") ? 2 : 0;
}

// The players on teams, and each one's team now.
std::vector<edict_t *> TeamPlayers(std::vector<int> &now)
{
	std::vector<edict_t *> players;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = Slot(i);
		if (Present(ent) && Core_Match_Playing(ent) && TeamOf(ent))
		{
			players.push_back(ent);
			now.push_back(TeamOf(ent));
		}
	}
	return players;
}
} // namespace

void Core_Teams_Frame()
{
	if (!match_lock)
		match_lock = gi.cvar("match_lock", "0", CVAR_NOFLAGS);
	// A new level: level.time starts again from zero, and every client
	// reconnects (back on their team: core/join.h).
	if (!begun || level.time < last_time || !Active())
	{
		teams.Reset();
		begun = Active();
		was_in_play = false;
		was_warmup = false;
	}
	last_time = level.time;
	if (!Active())
		return;

	Announce(teams.Sync(Snapshot()));

	// Locks belong to a match: on as it goes live (rounds and overtime don't
	// count), off when it's back in warmup.
	const bool owns = Core_Match_OwnsRules();
	const bool in_play = owns && Core_Match_InPlay();
	const bool warmup = owns && Core_Match_Warmup();
	if (in_play && !was_in_play)
		Announce(teams.GoLive(match_lock->integer != 0));
	if (warmup && !was_warmup)
		teams.Clear();
	was_in_play = in_play;
	was_warmup = warmup;
}

bool Core_Teams_ShuffleNow()
{
	if (!Active() || !Core_Match_OwnsRules() || Core_Match_InPlay() || level.intermissiontime)
		return false;
	std::vector<int> now;
	const std::vector<edict_t *> players = TeamPlayers(now);
	const std::vector<int> after = Core_Teams_Shuffle(now, mt_rand);
	// As going LIVE does: flags home, then everyone fresh at a spawn point.
	// Not stock's change of team, which is a suicide each and refuses a lock.
	if (ctf->integer)
	{
		CTFResetFlag(CTF_TEAM1);
		CTFResetFlag(CTF_TEAM2);
	}
	teams.Clear(); // a lock's roster is the old team; the next Sync settles the captains
	std::string names[3];
	for (size_t i = 0; i < players.size(); ++i)
	{
		edict_t *ent = players[i];
		ent->client->resp.ctf_team = after[i] == 1 ? CTF_TEAM1 : CTF_TEAM2;
		ent->client->resp.ctf_state = 0;
		char skin[MAX_INFO_VALUE] = {0};
		gi.Info_ValueForKey(ent->client->pers.userinfo, "skin", skin, sizeof(skin));
		CTFAssignSkin(ent, skin);
		PutClientInServer(ent);
		names[after[i]] += (names[after[i]].empty() ? "" : ", ") + Core_PlayerName(ent);
	}
	CTFDirtyTeamMenu();
	gi.LocBroadcast_Print(PRINT_HIGH, "Teams shuffled.\n");
	gi.Com_PrintFmt("Teams: shuffled: {}: {}; {}: {}\n", TeamName(1), names[1], TeamName(2), names[2]);
	return true;
}

bool Core_Teams_MayJoin(edict_t *ent, int team, bool tell)
{
	if (!Active() || teams.MayJoin(Core_PlayerKey(ent), team, (ent->svflags & SVF_BOT) != 0))
		return true;
	if (tell)
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is locked.\n", TeamName(team));
	return false;
}

void Core_Teams_Run(edict_t *ent, const char *cmd, edict_t *target)
{
	if (!Q_strcasecmp(cmd, "captain"))
		Captain(ent, target);
	else if (!Q_strcasecmp(cmd, "lockteam"))
		LockTeam(ent, true);
	else if (!Q_strcasecmp(cmd, "unlockteam"))
		LockTeam(ent, false);
	else if (!Q_strcasecmp(cmd, "teamkick") && target)
		Kick(ent, target);
	else if (!Q_strcasecmp(cmd, "readyteam"))
		ReadyTeam(ent);
}

bool Core_Teams_ClientCommand(edict_t *ent, const char *cmd)
{
	const bool ours = !Q_strcasecmp(cmd, "captain") || !Q_strcasecmp(cmd, "lockteam") ||
	                  !Q_strcasecmp(cmd, "unlockteam") || !Q_strcasecmp(cmd, "teamkick") ||
	                  !Q_strcasecmp(cmd, "readyteam");
	if (!ours || !deathmatch->integer)
		return false;
	if (!Active())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Captains and team locks are for team games.\n");
		return true;
	}
	edict_t *target = nullptr;
	const bool wants_target = !Q_strcasecmp(cmd, "teamkick") || !Q_strcasecmp(cmd, "captain");
	if (wants_target && gi.argc() > 1)
	{
		// The whole argument: a lobby name token decodes only as one (core/player_name.h).
		target = Named(gi.args());
		if (!target)
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "No one player called '{}'.\n", gi.args());
			return true;
		}
	}
	else if (!Q_strcasecmp(cmd, "teamkick"))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: teamkick <player>\n");
		return true;
	}
	Core_Teams_Run(ent, cmd, target);
	return true;
}

bool Core_Teams_ServerCommand(const char *cmd)
{
	const bool lock = !Q_strcasecmp(cmd, "lockteam");
	if (lock || !Q_strcasecmp(cmd, "unlockteam"))
	{
		const int team = ArgTeam();
		if (!Active() || !team)
		{
			gi.Com_PrintFmt("Usage: sv {} red|blue (team games only)\n", cmd);
			return true;
		}
		const TeamsResult r = lock ? teams.Lock(team) : teams.Unlock(team);
		if (r == TeamsResult::Ok)
		{
			gi.LocBroadcast_Print(PRINT_HIGH, lock ? "{} is locked.\n" : "{} is unlocked.\n", TeamName(team));
			gi.Com_PrintFmt("Teams: {} {} by the server\n", TeamName(team), lock ? "locked" : "unlocked");
		}
		else if (r == TeamsResult::Empty)
			gi.Com_PrintFmt("{} has nobody on it.\n", TeamName(team));
		else
			gi.Com_PrintFmt("{} is already {}.\n", TeamName(team), lock ? "locked" : "unlocked");
		return true;
	}
	if (!Q_strcasecmp(cmd, "shuffle"))
	{
		if (!Core_Teams_ShuffleNow())
			gi.Com_Print("Teams can only be shuffled in a team game's warmup.\n");
		return true;
	}
	if (Q_strcasecmp(cmd, "teams"))
		return false;
	if (!Active())
	{
		gi.Com_Print("Teams: not a team game\n");
		return true;
	}
	for (int t = 1; t <= 2; ++t)
	{
		const edict_t *captain = Find(teams.Captain(t));
		gi.Com_PrintFmt("{}: {}, captain {}\n", TeamName(t), teams.Locked(t) ? "locked" : "unlocked",
		                captain ? Core_PlayerName(captain).c_str() : "none");
		for (const std::string &key : teams.Roster(t))
		{
			const edict_t *ent = Find(key);
			gi.Com_PrintFmt("  roster: {}{}\n", ent ? Core_PlayerName(ent).c_str() : key.c_str(),
			                ent && TeamOf(ent) == t ? "" : " (away)");
		}
	}
	return true;
}

void Core_Teams_MenuInfo(const edict_t *ent, MenuView &v)
{
	if (!Active())
		return;
	const std::string key = Core_PlayerKey(ent);
	const int team = TeamOf(ent);
	for (int t = 1; t <= 2; ++t)
	{
		v.team_locked[t] = teams.Locked(t);
		v.team_may_join[t] = teams.MayJoin(key, t, (ent->svflags & SVF_BOT) != 0);
		const edict_t *captain = Find(teams.Captain(t));
		v.team_captain[t] = captain ? Core_PlayerName(captain) : std::string();
	}
	v.captain = team && teams.Captain(team) == key;
	if (!v.captain)
		return;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *mate = Slot(i);
		if (mate != ent && Present(mate) && !(mate->svflags & SVF_BOT) && TeamOf(mate) == team)
			v.teammates.push_back({(int)i, Core_PlayerName(mate)});
	}
}

std::string Core_Teams_ScoreboardMark(const edict_t *ent, int x, int y)
{
	const int team = Active() ? TeamOf(ent) : 0;
	if (!team || teams.Captain(team) != Core_PlayerKey(ent))
		return {};
	return fmt::format("xv {} yv {} string2 \"*\" ", x, y);
}
