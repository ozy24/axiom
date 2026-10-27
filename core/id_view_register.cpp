// Engine-facing player ID: stock keeps every player's name in
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
	// One line above the match status (yb -61).
	sb.ifstat(stat).xv(0).yb(-71);
	sb.sb << "loc_stat_cstring " << stat << ' ';
	sb.endifstat();
}

void Core_IdView_SetStats(edict_t *ent)
{
	const player_stat_t stat = IdViewStat();
	if (!stat)
		return;
	// STAT_CTF_ID_VIEW is the target's client number plus one, 0 for none.
	const int target = ent->client->ps.stats[STAT_CTF_ID_VIEW];
	ent->client->ps.stats[stat] = target ? CONFIG_CTF_PLAYER_NAME + target - 1 : 0;
}
