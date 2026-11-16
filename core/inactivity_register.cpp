// Engine-facing part of the inactivity check (core/inactivity.h): the
// setting, each client's last view angles, and the move to spectate.
#include "g_local.h"

#include "core/gametype.h"
#include "core/inactivity.h"
#include "core/inactivity_hooks.h"
#include "core/join_hooks.h"
#include "core/match_hooks.h"
#include "core/player_name.h"
#include "core/timeout_hooks.h"

#include <array>

namespace
{
cvar_t *g_inactivity;
Inactivity inactivity;
std::array<std::array<float, 2>, MAX_CLIENTS> last_angles{};
int64_t last_ms = -1; // the level time last seen: it goes back on a new level

void RegisterCvars()
{
	if (g_inactivity)
		return;
	// Seconds without input before a playing human is moved to spectate;
	// 0 = never. The server's, held across mode switches.
	g_inactivity = gi.cvar("g_inactivity", "120", CVAR_NOFLAGS);
}
} // namespace

void Core_Inactivity_Think(edict_t *ent, const usercmd_t *ucmd)
{
	if (!deathmatch->integer || !Core_Gametype_Active() || !ent->client)
		return;
	RegisterCvars();
	const int slot = (int)(ent - g_edicts) - 1;
	const int64_t now = level.time.milliseconds();
	if (now < last_ms)
		for (int i = 0; i < (int)game.maxclients; ++i)
			inactivity.Reset(i, now); // a new level: everyone starts again
	last_ms = now;

	const float angles[2] = {ucmd->angles[PITCH], ucmd->angles[YAW]};
	const bool active =
	    Core_Inactivity_Active(ucmd->buttons, ucmd->forwardmove, ucmd->sidemove, 0, angles, last_angles[slot].data());
	last_angles[slot] = {angles[0], angles[1]};

	// Only a playing human counts; a timeout or intermission holds everyone.
	if ((ent->svflags & SVF_BOT) || !Core_Match_Playing(ent) || Core_Timeout_Paused() || level.intermissiontime)
	{
		inactivity.Reset(slot, now);
		return;
	}
	switch (inactivity.Frame(slot, now, active, Inactivity::Seconds(g_inactivity->integer)))
	{
	case InactivityEvent::None:
		break;
	case InactivityEvent::Warn:
		gi.LocCenter_Print(ent, "Inactivity: you'll be moved to spectate in 10 seconds.");
		break;
	case InactivityEvent::Move:
	{
		const std::string name = Core_PlayerName(ent);
		Core_Join_Run(ent, false); // as their own `spectate`
		if (Core_Match_Playing(ent))
			break; // refused (the spectator limit): left playing, tried again later
		gi.LocCenter_Print(ent, "You've been moved to spectate for inactivity.");
		gi.LocBroadcast_Print(PRINT_HIGH, "{} was moved to spectate for inactivity.\n", ent->client->pers.netname);
		gi.Com_PrintFmt("Inactivity: {} moved to spectate\n", name.c_str());
		break;
	}
	}
}
