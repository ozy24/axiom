// Engine-facing player ID and chase name: stock keeps every player's name in
// CONFIG_CTF_PLAYER_NAME + n (ClientUserinfoChanged), so a stat pointing at
// that configstring lets loc_stat_cstring draw it centred. stat_pname, stock's
// token, only draws left-aligned.
#include "g_local.h"

#include "core/id_view_hooks.h"
#include "core/stat_slots.h"
#include "g_statusbar.h"

namespace
{
player_stat_t IdViewStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("id_view");
	return (player_stat_t)(slot ? *slot : 0);
}
} // namespace

void Core_IdView_Statusbar(statusbar_t &sb)
{
	const player_stat_t stat = IdViewStat();
	if (!stat)
		return;
	// One line above the match status (yb -58).
	sb.ifstat(stat).xv(0).yb(-68);
	sb.sb << "loc_stat_cstring " << stat << ' ';
	sb.endifstat();
}

void Core_IdView_SetStats(edict_t *ent)
{
	const player_stat_t stat = IdViewStat();
	if (!stat)
		return;
	// STAT_CTF_ID_VIEW is the target's client number plus one, 0 for none.
	// Team modes only: stock FFA shows no names under the crosshair.
	const int target = G_TeamplayEnabled() ? ent->client->ps.stats[STAT_CTF_ID_VIEW] : 0;
	ent->client->ps.stats[stat] = target ? CONFIG_CTF_PLAYER_NAME + target - 1 : 0;
}

void Core_IdView_SetSpectatorStats(edict_t *ent)
{
	const player_stat_t stat = IdViewStat();
	const edict_t *target = ent->client->chase_target;
	if (!target || !target->inuse)
		return;
	if (stat)
		ent->client->ps.stats[stat] = CONFIG_CTF_PLAYER_NAME + (int)(target - g_edicts) - 1;
	// The target's weapon icon, which stock shows a centre-handed player in
	// place of the gun model, duplicates the ammo icon over the chase view.
	ent->client->ps.stats[STAT_HELPICON] = 0;
}
