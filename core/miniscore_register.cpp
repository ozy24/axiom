// Engine-facing part of the miniscore (docs/design/hud.md): the statusbar
// rows, and each player's face and score in stock's CTF team-score stats.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match_hooks.h"
#include "core/miniscore.h"
#include "core/miniscore_hooks.h"
#include "core/stat_slots.h"
#include "g_statusbar.h"

namespace
{
// Distinct faces per level; the rest of the 512 image configstrings stay
// for the maps and items.
constexpr size_t kIconLimit = 32;

MiniscoreIcons icons(kIconLimit);
struct Face
{
	std::string skin;
	int image = 0;
	bool small = false; // a 24x24 icon (Core_Miniscore_SmallIcon)
};
std::array<Face, MAX_CLIENTS> faces; // per client slot, this level

// 1: the miniscore; 0: none (the layout keeps its rows, with no stats set).
cvar_t *g_hud_miniscore;

bool Shown()
{
	if (!g_hud_miniscore)
		g_hud_miniscore = gi.cvar("g_hud_miniscore", "1", CVAR_NOFLAGS);
	return g_hud_miniscore->integer != 0;
}

// Deathmatch outside team modes, under a registered gametype: team modes
// have stock's team scores in the same place, and single player, coop and
// Rogue's stock gamerules modes stay stock.
bool Active()
{
	return deathmatch->integer && Core_Gametype_Active() && !G_TeamplayEnabled();
}

// Two of the mod's stats (core/stat_slots.h): each row's face when it is a
// small icon, drawn centred in the slot; 0 if the pool is full (the small
// faces then take the usual stat, drawn from the slot's corner).
int SmallStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("miniscore_small", 2);
	return slot ? *slot : 0;
}

edict_t *ClientEdict(uint32_t i)
{
	return g_edicts + 1 + i;
}

// Client `i`'s face: their skin's icon, and whether it is a small one. An
// odd skin name, or one past the
// level's limit, gets stock's default skin's: every row has a picture, and
// the row shows only while it does (ifstat).
const Face &FaceOf(uint32_t i)
{
	char skin[MAX_INFO_VALUE] = {};
	if (!gi.Info_ValueForKey(ClientEdict(i)->client->pers.userinfo, "skin", skin, sizeof(skin)))
		Q_strlcpy(skin, "male/grunt", sizeof(skin)); // stock's default (ClientUserinfoChanged)
	Face &face = faces[i];
	if (face.skin != skin)
	{
		face.skin = skin;
		const std::string path = Core_Miniscore_IconPath(skin);
		face.image = !path.empty() && icons.Admit(path) ? gi.imageindex(path.c_str()) : 0;
		face.small = face.image && Core_Miniscore_SmallIcon(skin);
		if (!face.image)
			face.image = gi.imageindex("/players/male/grunt_i");
	}
	return face;
}

std::vector<MatchPlayer> Players()
{
	std::vector<MatchPlayer> out;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = ClientEdict(i);
		if (!ent->client || !Core_Match_Playing(ent))
			continue;
		MatchPlayer p;
		p.id = (int)i;
		p.playing = true;
		p.score = ent->client->resp.score;
		out.push_back(p);
	}
	return out;
}
} // namespace

void Core_Miniscore_Statusbar(statusbar_t &sb)
{
	// Once per level, as it spawns: image indexes belong to the level.
	icons.Reset();
	faces = {};
	Shown(); // registers g_hud_miniscore as the level spawns
	if (!Active())
		return;
	// Bottom right, where team modes show their team scores (yb -110 / -83),
	// spaced for the 32x32 face icons (the team pictures are 24x24): faces
	// at xr -34, the 24-high digits centred on them, one unit between rows.
	// Above the stock HUD's timer icon (yb -50) at 320 wide.
	// A 24x24 face (CTF skins) is in its own stat, drawn 4 in and 4 down so
	// it sits centred in the 32x32 slot; a row shows one or the other.
	sb.ifstat(STAT_CTF_TEAM1_PIC).yb(-116).xr(-34).pic(STAT_CTF_TEAM1_PIC);
	sb.yb(-112).xr(-86).num(3, STAT_CTF_TEAM1_CAPS).endifstat();
	sb.ifstat(STAT_CTF_TEAM2_PIC).yb(-83).xr(-34).pic(STAT_CTF_TEAM2_PIC);
	sb.yb(-79).xr(-86).num(3, STAT_CTF_TEAM2_CAPS).endifstat();
	if (const int small = SmallStat())
	{
		sb.ifstat((player_stat_t)small).yb(-112).xr(-30).pic((player_stat_t)small);
		sb.yb(-112).xr(-86).num(3, STAT_CTF_TEAM1_CAPS).endifstat();
		sb.ifstat((player_stat_t)(small + 1)).yb(-79).xr(-30).pic((player_stat_t)(small + 1));
		sb.yb(-79).xr(-86).num(3, STAT_CTF_TEAM2_CAPS).endifstat();
	}
}

void Core_Miniscore_SetStats(edict_t *ent)
{
	if (!Active())
		return;
	auto &stats = ent->client->ps.stats;
	stats[STAT_CTF_TEAM1_PIC] = stats[STAT_CTF_TEAM1_CAPS] = 0;
	stats[STAT_CTF_TEAM2_PIC] = stats[STAT_CTF_TEAM2_CAPS] = 0;
	const int small = SmallStat();
	if (small)
		stats[small] = stats[small + 1] = 0;
	if (!Core_Match_InPlay() || !Shown())
		return;

	const std::array<int, 2> rows = Core_Miniscore_Rows(Players());
	const player_stat_t pic[2] = {STAT_CTF_TEAM1_PIC, STAT_CTF_TEAM2_PIC};
	const player_stat_t num[2] = {STAT_CTF_TEAM1_CAPS, STAT_CTF_TEAM2_CAPS};
	for (int r = 0; r < 2; ++r)
	{
		if (rows[r] < 0)
			continue;
		const Face &face = FaceOf((uint32_t)rows[r]);
		stats[face.small && small ? small + r : pic[r]] = face.image;
		stats[num[r]] = ClientEdict((uint32_t)rows[r])->client->resp.score;
	}
}
