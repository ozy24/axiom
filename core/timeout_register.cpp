// Engine-facing part of match timeouts (core/timeout.h): the cvars, the
// commands, and holding the world still while one runs.
#include "g_local.h"

#include "core/match_hooks.h"
#include "core/player_name.h"
#include "core/timeout.h"
#include "core/timeout_hooks.h"

#include <string>

// id's (rerelease/p_client.cpp), not declared in a header.
bool HandleMenuMovement(edict_t *ent, usercmd_t *ucmd);

namespace
{
Timeout timeout;
gtime_t frozen_at; // level.time when the world stopped: any other value is a new level

cvar_t *match_timeout, *match_timeout_resume;

void RegisterCvars()
{
	if (match_timeout)
		return;
	match_timeout = gi.cvar("match_timeout", "0", CVAR_NOFLAGS);
	match_timeout_resume = gi.cvar("match_timeout_resume", "10", CVAR_NOFLAGS);
}

TimeoutConfig Config()
{
	RegisterCvars();
	TimeoutConfig c;
	c.length_ms = (int64_t)(std::max(0.f, match_timeout->value) * 1000);
	c.resume_ms = (int64_t)(std::max(0.f, match_timeout_resume->value) * 1000);
	return c;
}

// A match under way, run by the match flow: timeouts need one to pause.
bool InPlay()
{
	return deathmatch->integer && Core_Match_OwnsRules() && Core_Match_InPlay();
}

// The side a client calls for: their team in team modes, else themselves.
std::string Side(const edict_t *ent)
{
	if (G_TeamplayEnabled())
	{
		const ctfteam_t team = ent->client->resp.ctf_team;
		return team == CTF_TEAM1 ? "red" : team == CTF_TEAM2 ? "blue" : "";
	}
	return Core_PlayerKey(ent);
}

bool HumansPlaying()
{
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		const edict_t *ent = g_edicts + i;
		if (ent->inuse && ent->client && !(ent->svflags & SVF_BOT) && Core_Match_Playing(ent))
			return true;
	}
	return false;
}

void DirtyMenus()
{
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		edict_t *ent = g_edicts + i;
		if (ent->inuse && ent->client && ent->client->menu)
			ent->client->menudirty = true;
	}
}

void Sound(const char *path)
{
	gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(path), 1, ATTN_NONE, 0);
}

void Act(const std::vector<MatchAction> &actions)
{
	for (const MatchAction &a : actions)
	{
		if (a.kind == MatchActionKind::Sound)
			Sound(a.text.c_str());
		else if (a.kind == MatchActionKind::Announce)
		{
			for (uint32_t i = 1; i <= game.maxclients; ++i)
			{
				edict_t *ent = g_edicts + i;
				if (ent->inuse && ent->client)
					gi.LocCenter_Print(ent, "{}", a.text.c_str());
			}
		}
	}
}

// The world stops: the timeout begins. `ent` null: the server's.
void Begin(const edict_t *ent)
{
	frozen_at = level.time;
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		edict_t *other = g_edicts + i;
		if (!other->inuse || !other->client)
			continue;
		if (ent)
			gi.LocCenter_Print(other, "{} called a timeout", ent->client->pers.netname);
		else
			gi.LocCenter_Print(other, "The server called a timeout");
	}
	Sound("world/klaxon2.wav");
	DirtyMenus();
	gi.Com_PrintFmt("Timeout: called by {}{}{}, {} s\n", ent ? Core_PlayerName(ent) : std::string("the server"),
	                ent && G_TeamplayEnabled() ? " for " : "", ent && G_TeamplayEnabled() ? Side(ent) : "",
	                (timeout.LeftMs() + 999) / 1000);
}

void Call(edict_t *ent)
{
	const TimeoutError e = timeout.Call(Side(ent), InPlay(), Core_Match_Playing(ent) && !Side(ent).empty(), Config());
	if (e != TimeoutError::None)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", Core_Timeout_ErrorText(e).c_str());
		return;
	}
	Begin(ent);
}

void TimeIn(edict_t *ent)
{
	const TimeoutError e = timeout.TimeIn(ent ? Side(ent) : Timeout::kServer);
	if (e != TimeoutError::None)
	{
		const std::string text = Core_Timeout_ErrorText(e);
		if (ent)
			gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", text.c_str());
		else
			gi.Com_PrintFmt("{}\n", text);
		return;
	}
	if (ent)
		gi.LocBroadcast_Print(PRINT_HIGH, "{} ends the timeout.\n", ent->client->pers.netname);
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "The server ends the timeout.\n");
	gi.Com_PrintFmt("Timeout: ended early by {}\n", ent ? Core_PlayerName(ent) : std::string("the server"));
}

// Drops the timeout under way (a new level; nobody left to play).
void Drop(const char *why)
{
	if (!timeout.Paused())
		return;
	timeout.Cancel();
	DirtyMenus();
	gi.Com_PrintFmt("Timeout: dropped ({})\n", why);
}
} // namespace

bool Core_Timeout_Paused()
{
	if (!timeout.Paused())
		return false;
	// Level time never moves while the world is still: another value is a
	// new level (a `map` or `gamemap` from the console), and its world runs.
	if (level.time != frozen_at || !deathmatch->integer)
	{
		Drop("new level");
		return false;
	}
	return true;
}

bool Core_Timeout_Frame()
{
	if (!Core_Timeout_Paused())
		return false;
	if (!HumansPlaying() && !gi.cvar("match_allow_no_humans", "0", CVAR_NOFLAGS)->integer)
	{
		Drop("no players left"); // the match flow goes back to warmup
		return false;
	}
	Act(timeout.Tick(FRAME_TIME_MS.milliseconds()));
	if (!timeout.Paused())
	{
		gi.Com_Print("Timeout: over, play resumes\n");
		DirtyMenus();
	}
	Core_Match_PausedFrame();
	return true;
}

bool Core_Timeout_Think(edict_t *ent, const usercmd_t *ucmd)
{
	if (!Core_Timeout_Paused())
		return false;
	gclient_t *client = ent->client;
	client->ps.pmove.pm_type = PM_FREEZE;
	client->resp.cmd_angles = ucmd->angles;
	// ClientThink has latched this frame's presses: none carry into play (a
	// fire press would shoot on the first frame back).
	client->latched_buttons = BUTTON_NONE;
	if (client->menu)
		HandleMenuMovement(ent, const_cast<usercmd_t *>(ucmd));
	return true;
}

bool Core_Timeout_ClientCommand(edict_t *ent, const char *cmd)
{
	if (!deathmatch->integer)
		return false;
	if (!Q_strcasecmp(cmd, "time-out") || !Q_strcasecmp(cmd, "timeout"))
	{
		Call(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "time-in") || !Q_strcasecmp(cmd, "timein"))
	{
		TimeIn(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "kill") && Core_Timeout_Paused())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Not during a timeout.\n");
		return true;
	}
	return false;
}

bool Core_Timeout_ServerCommand(const char *cmd)
{
	if (!Q_strcasecmp(cmd, "timein"))
	{
		TimeIn(nullptr);
		return true;
	}
	if (Q_strcasecmp(cmd, "timeout"))
		return false;
	const TimeoutError e = timeout.Call(Timeout::kServer, InPlay(), true, Config());
	if (e != TimeoutError::None)
		gi.Com_PrintFmt("{}\n", Core_Timeout_ErrorText(e));
	else
		Begin(nullptr);
	return true;
}

void Core_Timeout_NewMatch()
{
	if (timeout.Paused())
		gi.Com_Print("Timeout: dropped (the match restarts)\n");
	timeout.NewMatch();
}

const char *Core_Timeout_Status()
{
	static std::string text;
	text = Core_Timeout_Paused() ? timeout.Status() : "";
	return text.c_str();
}

int Core_Timeout_CountdownSeconds()
{
	return Core_Timeout_Paused() ? timeout.CountdownSeconds() : 0;
}

void Core_Timeout_MenuInfo(const edict_t *ent, bool &can_call, bool &can_end)
{
	const TimeoutConfig c = Config();
	const std::string side = Side(ent);
	const bool playing = Core_Match_Playing(ent) && !side.empty();
	can_call = c.length_ms > 0 && InPlay() && playing && !Core_Timeout_Paused() && !timeout.Used(side);
	can_end = playing && timeout.Phase() == TimeoutPhase::Paused && Core_Timeout_Paused() && timeout.Side() == side;
}

void Core_Timeout_PrintStatus()
{
	if (!Core_Timeout_Paused())
		return;
	const std::string &side = timeout.Side();
	gi.Com_PrintFmt("  timeout: {} ({})\n", timeout.Status(), side.empty() ? "server" : side);
	// Where each player stands: nobody moves until it's over (the harness's check).
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		const edict_t *ent = g_edicts + i;
		if (ent->inuse && ent->client && Core_Match_Playing(ent))
			gi.Com_PrintFmt("    {} at {} {} {}\n", Core_PlayerName(ent).c_str(), (int)ent->s.origin[0],
			                (int)ent->s.origin[1], (int)ent->s.origin[2]);
	}
}
