// Engine-facing part of the match flow (docs/design/match-flow.md): builds the
// state machine's input from the level each frame, carries out its actions,
// and handles the ready / notready commands.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match.h"
#include "core/host_files.h"
#include "core/match_record.h"
#include "core/player_name.h"
#include "core/stat_slots.h"
#include "g_statusbar.h"
#include "mod_identity.h"
#include "core/match_hooks.h"

#include <array>
#include <optional>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

// id's, not declared in g_local.h. A plain declaration: THINK() would register
// it for saves a second time.
void DoRespawn(edict_t *ent);

namespace
{
Match match;
bool begun = false;
bool ended_this_level = false; // the gametype's match_ended has run
std::string end_reason;        // why the match flow ended the match ("" = stock rules)
cvar_t *g_matchlog_dir;
gtime_t last_time;
std::array<bool, MAX_CLIENTS> ready{};

cvar_t *match_warmup, *match_ready, *match_minplayers, *match_warmup_time, *match_countdown, *match_overtime,
    *match_rounds, *match_round_countdown, *match_round_pause, *match_suicide_point;

void RegisterCvars()
{
	if (match_warmup)
		return;
	match_warmup = gi.cvar("match_warmup", "0", CVAR_NOFLAGS);
	match_ready = gi.cvar("match_ready", "1", CVAR_NOFLAGS);
	match_minplayers = gi.cvar("match_minplayers", "2", CVAR_NOFLAGS);
	match_warmup_time = gi.cvar("match_warmup_time", "0", CVAR_NOFLAGS);
	match_countdown = gi.cvar("match_countdown", "10", CVAR_NOFLAGS);
	match_overtime = gi.cvar("match_overtime", "0", CVAR_NOFLAGS);
	match_rounds = gi.cvar("match_rounds", "0", CVAR_NOFLAGS);
	match_round_countdown = gi.cvar("match_round_countdown", "5", CVAR_NOFLAGS);
	match_round_pause = gi.cvar("match_round_pause", "2", CVAR_NOFLAGS);
	match_suicide_point = gi.cvar("match_suicide_point", "0", CVAR_NOFLAGS);
	// Where finished-match records go (relative to the server's working
	// directory); empty = none.
	g_matchlog_dir = gi.cvar("g_matchlog_dir", "", CVAR_NOFLAGS);
}

MatchConfig ReadConfig()
{
	MatchConfig c;
	// Only deathmatch has matches; single player and coop stay stock.
	c.enabled = deathmatch->integer && match_warmup->integer;
	c.ready_required = match_ready->integer != 0;
	c.min_players = match_minplayers->integer;
	c.warmup_time_ms = (int64_t)(match_warmup_time->value * 1000);
	c.countdown_ms = (int64_t)(match_countdown->value * 1000);
	c.overtime = match_overtime->integer;
	c.rounds_on_frag = match_rounds->integer != 0;
	c.round_countdown_ms = (int64_t)(match_round_countdown->value * 1000);
	c.round_pause_ms = (int64_t)(match_round_pause->value * 1000);
	c.timelimit_ms = (int64_t)(timelimit->value * 60000);
	c.fraglimit = fraglimit->integer;
	// TDM and CTF run on stock's team code (Core_Gametype_PreInit): the lead
	// is the team score, and in TDM the fraglimit is too (CheckEndTDMLevel).
	c.teams = G_TeamplayEnabled();
	c.fraglimit_on_teams = teamplay->integer != 0;
	return c;
}

edict_t *ClientEdict(uint32_t i)
{
	return g_edicts + 1 + i;
}

// In the game as a player: begun (pers.spawned, set by ClientBegin), not a
// spectator, and in team modes on a team. A client that connected but never
// began - Q2REX's bots in a bot-only match - is not, so the arena reset never
// puts it in the world.
bool Playing(const edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->pers.connected && ent->client->pers.spawned &&
	       !ent->client->resp.spectator && (!G_TeamplayEnabled() || ent->client->resp.ctf_team != CTF_NOTEAM);
}

MatchInput Snapshot()
{
	MatchInput in;
	in.now_ms = level.time.milliseconds();
	const GametypeDef *gt = Core_Gametype_Active();
	if (const char *reason = gt && gt->match_over ? gt->match_over() : nullptr)
	{
		in.gametype_over = true;
		in.gametype_over_reason = reason;
	}
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = ClientEdict(i);
		if (!ent->inuse || !ent->client || !ent->client->pers.connected)
			continue;
		MatchPlayer p;
		p.id = (int)i;
		p.playing = Playing(ent);
		// Bots can't type `ready`; they never hold a match up.
		p.ready = ready[i] || (ent->svflags & SVF_BOT);
		p.score = ent->client->resp.score;
		p.team = ent->client->resp.ctf_team == CTF_TEAM1 ? 1 : ent->client->resp.ctf_team == CTF_TEAM2 ? 2 : 0;
		in.players.push_back(p);
	}
	if (G_TeamplayEnabled())
		in.team_scores = ctf->integer ? G_TeamCaptures() : G_TeamFrags();
	return in;
}

void CenterAll(const std::string &text)
{
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (ent->inuse && ent->client)
			gi.LocCenter_Print(ent, "{}", text.c_str());
	}
}

bool Transient(const edict_t *ent)
{
	static const char *const kinds[] = {"rocket",        "grenade", "hand_grenade",   "bolt",       "bfg blast",
	                                    "disint ball",   "tracker", "prox_mine",      "prox_field", "tesla_mine",
	                                    "tesla trigger", "nuke",    "food_cube_trap", "pain daemon"};
	if (ent->item && ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
		return true; // dropped weapons, packs, the dead's items
	if (!ent->classname)
		return false;
	for (const char *k : kinds)
	{
		if (!strcmp(ent->classname, k))
			return true;
	}
	return false;
}

// Puts the level back to its start: no projectiles or dropped items, every map
// item present. With `players`, also every player fresh at a spawn point with a
// zero score (going LIVE); without, players and scores are left alone (between
// rounds, where RoundHold / RoundStart handle the players).
void ResetArena(bool players)
{
	if (ctf->integer)
	{
		// Flags home first: a dropped flag is freed, a taken one shown again.
		CTFResetFlag(CTF_TEAM1);
		CTFResetFlag(CTF_TEAM2);
	}
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
	{
		edict_t *ent = g_edicts + i;
		if (ent->inuse && Transient(ent))
			G_FreeEdict(ent);
	}
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
	{
		edict_t *ent = g_edicts + i;
		if (!ent->inuse || !ent->item || ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
			continue;
		// Team chains (one of several items spawns) respawn once, via the master.
		if (ent->team && ent->teammaster != ent)
			continue;
		bool hidden = (ent->svflags & SVF_NOCLIENT) != 0;
		if (ent->team)
		{
			hidden = true;
			for (edict_t *m = ent; m; m = m->chain)
				hidden = hidden && (m->svflags & SVF_NOCLIENT);
		}
		if (hidden)
		{
			DoRespawn(ent);
			for (edict_t *m = ent; m; m = m->team ? m->chain : nullptr)
				m->nextthink = 0_ms; // cancel any pending respawn timer
		}
	}
	if (!players)
		return;
	if (G_TeamplayEnabled())
		G_ResetTeamScores();
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (!ent->inuse || !ent->client)
			continue;
		ent->client->resp.score = 0;
		if (Playing(ent))
			PutClientInServer(ent);
	}
}

// Warmup and the countdown are free play: every frag, suicide and capture is
// undone in the frame it happens, before any client sees it.
// The HUD's match-state line: its text lives in the first general configstring
// stock leaves free; each player's stat points at it while there is text.
constexpr int kStateConfigstring = CONFIG_LAST;
constexpr int kClockConfigstring = CONFIG_LAST + 1;
static_assert(kClockConfigstring < CS_GENERAL + MAX_GENERAL, "no free general configstring");
std::string state_text; // what the configstrings hold now
std::string clock_text;

// PLAYTEST ONLY (2026-10-27): where the live clock goes, to compare on a real
// client. 0 = in the state line; 1 bottom-right, 2 bottom-left, 3 top-right
// under the frags, 4 above the stock row, 5 top centre, 6 the pickup row
// (right of the item name). Read at map load.
// Replaced by the chosen placement before this branch is merged.
cvar_t *match_hud_clock;

player_stat_t StateStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("match_state");
	return (player_stat_t)(slot ? *slot : 0);
}

// The HUD's big countdown digits: seconds left in the match or round countdown.
player_stat_t CountdownStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("match_countdown");
	return (player_stat_t)(slot ? *slot : 0);
}

player_stat_t ClockStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("match_clock");
	return (player_stat_t)(slot ? *slot : 0);
}

void PublishState()
{
	const std::string text = match.OwnsRules() && !level.intermissiontime ? match.StateText(Snapshot()) : "";
	const bool clock_apart = match_hud_clock && match_hud_clock->integer > 0 && match.ShowsClock();
	const std::string state = clock_apart ? "" : text, clock = clock_apart ? text : "";
	if (state != state_text)
	{
		state_text = state;
		gi.configstring(kStateConfigstring, state.c_str());
	}
	if (clock != clock_text)
	{
		clock_text = clock;
		gi.configstring(kClockConfigstring, clock.c_str());
	}
}

void ZeroScores()
{
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (ent->inuse && ent->client)
			ent->client->resp.score = 0;
	}
	if (G_TeamplayEnabled())
		G_ResetTeamScores();
}

// The round is over: a playing client who is dead stays dead (no respawn on a
// button press) until RoundRespawn. The winner plays on. Run every frame of
// the pause, so a death during it is held too.
void HoldDead()
{
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (Playing(ent) && ent->deadflag)
			ent->client->respawn_time = level.time + 3600_sec;
	}
}

// After the pause: every playing client back in at a spawn point with a fresh
// loadout - the dead through respawn (which leaves their body), the living
// straight to a spawn. Frozen until RoundStart (Core_Match_Frozen).
void RoundRespawn()
{
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (!Playing(ent))
			continue;
		ent->client->respawn_time = level.time;
		if (ent->deadflag)
			respawn(ent);
		else
			PutClientInServer(ent);
	}
}

std::string UtcNow()
{
	const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
	return buf;
}

// At intermission, with g_matchlog_dir set: one JSON record per finished match.
void WriteMatchRecord()
{
	if (!g_matchlog_dir || !*g_matchlog_dir->string)
		return;
	MatchRecord r;
	r.mod_name = MOD_NAME;
	r.mod_id = MOD_ID;
	r.mod_version = MOD_VERSION;
	r.mod_build = MOD_BUILD;
	if (const GametypeDef *gt = Core_Gametype_Active())
		r.gametype = gt->id;
	else
		r.gametype = G_Fmt("stock gamerules {}", gamerules->integer).data();
	r.ruleset = gi.cvar("g_ruleset", "stock", CVAR_SERVERINFO)->string;
	r.map = level.mapname;
	r.ended_at_utc = UtcNow();
	r.reason = end_reason.empty() ? "rules" : end_reason;
	r.live_ms = level.time.milliseconds() - match.LiveSinceMs();
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = ClientEdict(i);
		if (!ent->inuse || !ent->client || !ent->client->pers.connected)
			continue;
		const ctfteam_t team = ent->client->resp.ctf_team;
		r.players.push_back({Core_PlayerName(ent), ent->client->resp.score, Playing(ent), (ent->svflags & SVF_BOT) != 0,
		                     !G_TeamplayEnabled() ? ""
		                     : team == CTF_TEAM1  ? "red"
		                     : team == CTF_TEAM2  ? "blue"
		                                          : ""});
	}
	if (G_TeamplayEnabled())
	{
		const std::array<int, 2> scores = ctf->integer ? G_TeamCaptures() : G_TeamFrags();
		r.teams = {{"red", scores[0]}, {"blue", scores[1]}};
	}
	const std::string name = Core_MatchRecord_FileName(r.ended_at_utc, r.map);
	const std::string json = Core_MatchRecord_ToJson(r);
	// A relative directory is a game path: with the engine's file access it
	// goes under the game directory, beside the saves (decision 0011).
	const std::filesystem::path dir(g_matchlog_dir->string);
	const std::string game_path = Core_GamePath(g_matchlog_dir->string, name);
	if (!dir.has_root_path() && !game_path.empty() && Core_HostFiles_Available())
	{
		if (Core_HostFiles_Write(game_path, json))
			gi.Com_PrintFmt("Match record: {} (game directory)\n", game_path.c_str());
		else
			gi.Com_PrintFmt("Match record: couldn't write {} (game directory)\n", game_path.c_str());
		return;
	}
	// Otherwise a plain path, relative to the server's working directory.
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	const std::filesystem::path file = dir / name;
	std::ofstream out(file, std::ios::binary);
	out << json;
	if (out)
		gi.Com_PrintFmt("Match record: {}\n", file.generic_string().c_str());
	else
		gi.Com_PrintFmt("Match record: couldn't write {}\n", file.generic_string().c_str());
}

void Act(const std::vector<MatchAction> &actions)
{
	for (const MatchAction &a : actions)
	{
		switch (a.kind)
		{
		case MatchActionKind::Announce:
			CenterAll(a.text);
			break;
		case MatchActionKind::Countdown:
			break; // shown by the HUD's big digits (Core_Match_SetStats)
		case MatchActionKind::ResetArena:
			ResetArena(a.value == 0);
			break;
		case MatchActionKind::RoundHold:
			HoldDead();
			break;
		case MatchActionKind::RoundRespawn:
			RoundRespawn();
			break;
		case MatchActionKind::RoundStart:
			break; // the release: Core_Match_Frozen is false from here

		case MatchActionKind::EndMatch:
			end_reason = a.text;
			gi.Com_PrintFmt("Match: ended ({})\n", a.text.c_str());
			EndDMLevel();
			break;
		}
	}
}
} // namespace

void Core_Match_Frame()
{
	RegisterCvars();
	// A new level: level.time starts again from zero.
	if (!begun || level.time < last_time)
	{
		match = Match(ReadConfig());
		match.Begin(level.time.milliseconds());
		ready.fill(false);
		begun = true;
		ended_this_level = false;
		end_reason.clear();
		state_text.clear(); // configstrings start empty on a new level
		clock_text.clear();
		if (match.OwnsRules())
			gi.Com_PrintFmt("Match: {}\n", MatchStateName(match.State()));
	}
	last_time = level.time;

	// The gametype's own per-frame work and end-of-match hook run whether or
	// not the match flow owns the rules (stock rules can end the level too).
	const GametypeDef *gt = deathmatch->integer ? Core_Gametype_Active() : nullptr;
	if (gt && gt->frame)
		gt->frame();
	if (deathmatch->integer && level.intermissiontime && !ended_this_level)
	{
		ended_this_level = true;
		WriteMatchRecord(); // before match_ended, which may rotate players
		if (gt && gt->match_ended)
			gt->match_ended();
	}
	if (!match.OwnsRules() || level.intermissiontime)
	{
		PublishState();
		return;
	}

	const MatchState before = match.State();
	const std::vector<MatchAction> actions = match.Update(Snapshot());
	if (match.State() != before)
		gi.Com_PrintFmt("Match: {} -> {}\n", MatchStateName(before), MatchStateName(match.State()));
	Act(actions);
	if (!match.ScoresCount())
		ZeroScores();
	if (match.State() == MatchState::RoundOver)
		HoldDead();
	PublishState();
}

bool Core_Match_OwnsRules()
{
	return begun && match.OwnsRules();
}

bool Core_Match_ServerCommand(const char *cmd)
{
	if (Q_strcasecmp(cmd, "match"))
		return false;
	gi.Com_PrintFmt("Match: {}\n", Core_Match_OwnsRules() ? MatchStateName(match.State()) : "off (stock rules)");
	if (G_TeamplayEnabled())
	{
		const std::array<int, 2> scores = ctf->integer ? G_TeamCaptures() : G_TeamFrags();
		gi.Com_PrintFmt("  teams: red {}, blue {}\n", scores[0], scores[1]);
	}
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = ClientEdict(i);
		if (!ent->inuse || !ent->client || !ent->client->pers.connected)
			continue;
		gi.Com_PrintFmt("  {:4} {}{}\n", ent->client->resp.score, Core_PlayerName(ent).c_str(),
		                Playing(ent) ? "" : " (not playing)");
	}
	return true;
}

bool Core_Match_SuicidePoint(edict_t *self)
{
	RegisterCvars();
	if (!match_suicide_point->integer || !deathmatch->integer || G_TeamplayEnabled() || !self->client)
		return false;
	const int opponent = Core_SuicideOpponent(Snapshot().players, (int)(self - g_edicts) - 1);
	if (opponent < 0)
		return false;
	ClientEdict((uint32_t)opponent)->client->resp.score++;
	return true;
}

void Core_Match_Statusbar(statusbar_t &sb)
{
	if (!deathmatch->integer || !StateStat() || !CountdownStat())
		return;
	// The state line, bottom centre: centred text from a configstring, two
	// lines in warmup ("WARMUP" / "1/2 READY"), so it starts at -88 to stay
	// clear of CHASING / SPECTATOR MODE (yb -68 / -58). Stock tokens only, and
	// no `ifbit`.
	const player_stat_t state = StateStat(), countdown = CountdownStat();
	sb.ifstat(state).xv(0).yb(-88);
	sb.sb << "loc_stat_cstring " << state << ' ';
	sb.endifstat();
	// The countdown in big HUD digits, mid-screen (MuffMode's placement: num 3
	// right-aligns in a 50 px field, so xv 118 centres one digit).
	sb.ifstat(countdown).xv(118).yb(-256).num(3, countdown).endifstat();

	// PLAYTEST ONLY: the live clock's trial placements (match_hud_clock).
	match_hud_clock = gi.cvar("match_hud_clock", "0", CVAR_NOFLAGS);
	const player_stat_t clock = ClockStat();
	if (!clock || match_hud_clock->integer <= 0)
		return;
	// PLAYTEST ONLY: match_hud_clock_dy nudges any placement (negative = up).
	const int dy = gi.cvar("match_hud_clock_dy", "0", CVAR_NOFLAGS)->integer;
	sb.ifstat(clock);
	switch (match_hud_clock->integer)
	{
	case 1: // bottom-right corner
		sb.xr(-8).yb(-16 + dy);
		sb.sb << "loc_stat_rstring " << clock << ' ';
		break;
	case 2: // bottom-left corner
		sb.xl(8).yb(-16 + dy);
		sb.sb << "loc_stat_string " << clock << ' ';
		break;
	case 3: // top-right, under the frag counter
		sb.xr(-8).yt(30 + dy);
		sb.sb << "loc_stat_rstring " << clock << ' ';
		break;
	case 6: // the pickup row, right of the item name, left of the powerup timer
		sb.xv(254).yb(-42 + dy);
		sb.sb << "loc_stat_rstring " << clock << ' ';
		break;
	case 4: // just above the stock row, centred
		sb.xv(0).yb(-40 + dy);
		sb.sb << "loc_stat_cstring " << clock << ' ';
		break;
	default: // 5: top centre
		sb.xv(0).yt(8 + dy);
		sb.sb << "loc_stat_cstring " << clock << ' ';
		break;
	}
	sb.endifstat();
}

void Core_Match_SetStats(edict_t *ent)
{
	if (StateStat())
		ent->client->ps.stats[StateStat()] = state_text.empty() ? 0 : kStateConfigstring;
	if (ClockStat())
		ent->client->ps.stats[ClockStat()] = clock_text.empty() ? 0 : kClockConfigstring;
	if (CountdownStat())
		ent->client->ps.stats[CountdownStat()] =
		    Core_Match_OwnsRules() ? match.CountdownSeconds(level.time.milliseconds()) : 0;
}

bool Core_Match_Frozen(const edict_t *ent)
{
	return Core_Match_OwnsRules() && match.Frozen() && Playing(ent);
}

bool Core_Match_ClockEnd(gtime_t &end)
{
	if (!Core_Match_OwnsRules())
	{
		end = gtime_t::from_min(timelimit->value);
		return timelimit->value != 0;
	}
	const int64_t ms = match.ClockEndMs();
	end = gtime_t::from_ms(ms);
	return ms != 0;
}

bool Core_Match_ClientCommand(edict_t *ent, const char *cmd)
{
	// The active gametype's own commands first (Duel's `queue`).
	if (const GametypeDef *gt = deathmatch->integer ? Core_Gametype_Active() : nullptr)
	{
		if (gt->client_command && gt->client_command(ent, cmd))
			return true;
	}

	const bool want_ready = !Q_strcasecmp(cmd, "ready");
	if (!want_ready && Q_strcasecmp(cmd, "notready"))
		return false;
	// Match flow off: stock's commands (CTF competition mode's own ready-up).
	if (!Core_Match_OwnsRules())
		return false;
	if (match.State() != MatchState::Warmup && match.State() != MatchState::Countdown)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "There is no match warmup.\n");
		return true;
	}
	if (!Playing(ent))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Join the game first.\n");
		return true;
	}
	const uint32_t slot = (uint32_t)(ent - g_edicts) - 1;
	if (ready[slot] != want_ready)
	{
		ready[slot] = want_ready;
		gi.LocBroadcast_Print(PRINT_HIGH, want_ready ? "{} is ready.\n" : "{} is not ready.\n",
		                      ent->client->pers.netname);
	}
	return true;
}
