// Jump's engine-facing half: the hooks it installs and its one command.
// Only this file touches the engine (gi, level, edicts).
#include "g_local.h"
#include "gametypes/jump/jump.h"

#include <array>

namespace
{
// Per-player data of the gametype's own, kept here rather than in id's
// structs (so saves are untouched): when each player started their run.
std::array<gtime_t, MAX_CLIENTS> run_started{};

int ChangeDamage(edict_t *, edict_t *attacker, int damage, mod_t mod)
{
	return Jump_Hurts(attacker && attacker->client, mod.id == MOD_FALLING) ? damage : 0;
}

void ClientBegin(edict_t *ent)
{
	run_started[ent->s.number - 1] = level.time;
}
} // namespace

void Jump_Install(dm_game_rt &hooks)
{
	hooks.ChangeDamage = ChangeDamage;
	hooks.ClientBegin = ClientBegin;
}

// `jumptime start` starts a run; `jumptime` shows how long it has run.
bool Jump_ClientCommand(edict_t *ent, const char *cmd)
{
	if (Q_strcasecmp(cmd, "jumptime"))
		return false;
	gtime_t &started = run_started[ent->s.number - 1];
	if (!Q_strcasecmp(gi.argv(1), "start"))
	{
		started = level.time;
		gi.LocClient_Print(ent, PRINT_HIGH, "Run started.\n");
		return true;
	}
	const std::string text = Jump_TimeText((level.time - started).milliseconds());
	gi.LocClient_Print(ent, PRINT_HIGH, "Run time: {}\n", text.c_str());
	return true;
}
