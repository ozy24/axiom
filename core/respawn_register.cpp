// Engine-facing part of respawning (core/respawn.h): the forced respawn's
// time per client, and the hooks in player_die and ClientThink.
#include "g_local.h"

#include "core/gametype.h"
#include "core/player_name.h"
#include "core/respawn.h"
#include "core/respawn_hooks.h"

#include <array>

namespace
{
std::array<int64_t, MAX_CLIENTS> forced_at{}; // level ms; 0 = not forced

bool Active()
{
	return deathmatch->integer && Core_Gametype_Active();
}

uint32_t Slot(const edict_t *ent)
{
	return (uint32_t)(ent - g_edicts) - 1;
}
} // namespace

void Core_Respawn_Died(edict_t *self)
{
	if (!Active() || !self->client)
		return;
	const int64_t now = level.time.milliseconds();
	const RespawnTimes t = Core_Respawn_Times(now, g_dm_force_respawn->integer != 0, g_dm_force_respawn_time->value);
	self->client->respawn_time = gtime_t::from_ms(t.may_ms);
	forced_at[Slot(self)] = t.forced_ms;
}

bool Core_Respawn_Forced(const edict_t *ent)
{
	if (!Active())
		return deathmatch->integer && g_dm_force_respawn->integer; // stock's
	const int64_t at = forced_at[Slot(ent)];
	if (!at || level.time.milliseconds() < at)
		return false;
	// Only a player who didn't press fire gets here (ClientThink tests the
	// button first): the console sees who sat it out.
	gi.Com_PrintFmt("Respawn: {} respawned by the server (g_dm_force_respawn)\n", Core_PlayerName(ent));
	return true;
}
