// Engine-facing part of the miniscore (docs/design/hud.md): the statusbar
// rows, and each player's face and score in stock's CTF team-score stats.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match_hooks.h"
#include "core/miniscore.h"
#include "core/miniscore_hooks.h"
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
};
std::array<Face, MAX_CLIENTS> faces; // per client slot, this level

// Deathmatch outside team modes, under a registered gametype: team modes
// have stock's team scores in the same place, and single player, coop and
// Rogue's stock gamerules modes stay stock.
bool Active()
{
	return deathmatch->integer && Core_Gametype_Active() && !G_TeamplayEnabled();
}

edict_t *ClientEdict(uint32_t i)
{
	return g_edicts + 1 + i;
}

// The image of client `i`'s skin icon. An odd skin name, or one past the
// level's limit, gets stock's default skin's: every row has a picture, and
// the row shows only while it does (ifstat).
int FaceImage(uint32_t i)
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
		if (!face.image)
			face.image = gi.imageindex("/players/male/grunt_i");
	}
	return face.image;
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
	if (!Active())
		return;
	// Bottom right, where team modes show their team scores (yb -110 / -83),
	// spaced for the 32x32 face icons (the team pictures are 24x24): faces
	// at xr -34, the 24-high digits centred on them, one unit between rows.
	// Above the stock HUD's timer icon (yb -50) at 320 wide.
	sb.ifstat(STAT_CTF_TEAM1_PIC).yb(-116).xr(-34).pic(STAT_CTF_TEAM1_PIC);
	sb.yb(-112).xr(-86).num(3, STAT_CTF_TEAM1_CAPS).endifstat();
	sb.ifstat(STAT_CTF_TEAM2_PIC).yb(-83).xr(-34).pic(STAT_CTF_TEAM2_PIC);
	sb.yb(-79).xr(-86).num(3, STAT_CTF_TEAM2_CAPS).endifstat();
}

void Core_Miniscore_SetStats(edict_t *ent)
{
	if (!Active())
		return;
	auto &stats = ent->client->ps.stats;
	stats[STAT_CTF_TEAM1_PIC] = stats[STAT_CTF_TEAM1_CAPS] = 0;
	stats[STAT_CTF_TEAM2_PIC] = stats[STAT_CTF_TEAM2_CAPS] = 0;
	if (!Core_Match_InPlay())
		return;

	const std::array<int, 2> rows = Core_Miniscore_Rows(Players());
	const player_stat_t pic[2] = {STAT_CTF_TEAM1_PIC, STAT_CTF_TEAM2_PIC};
	const player_stat_t num[2] = {STAT_CTF_TEAM1_CAPS, STAT_CTF_TEAM2_CAPS};
	for (int r = 0; r < 2; ++r)
	{
		if (rows[r] < 0)
			continue;
		stats[pic[r]] = FaceImage((uint32_t)rows[r]);
		stats[num[r]] = ClientEdict((uint32_t)rows[r])->client->resp.score;
	}
}
