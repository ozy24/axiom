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
#include "core/maps_hooks.h"
#include "core/match_hooks.h"
#include "core/vote_hooks.h"

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
	c.join_grace_ms = 10000;
	c.join_wait_ms = 30000;
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
		if (!ent->client || !ent->client->pers.connected)
			continue;
		MatchPlayer p;
		p.id = (int)i;
		// Connected, not yet begun: loading the map (not inuse until ClientBegin).
		p.joining = !ent->client->pers.spawned && !(ent->svflags & SVF_BOT);
		if (!ent->inuse && !p.joining)
			continue;
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

// The HUD's match status and clock (docs/design/hud.md): each text lives in a
// general configstring stock leaves free; a player's stat points at it while
// there is text, so an empty one isn't drawn.
constexpr int kStatusConfigstring = CONFIG_LAST;
constexpr int kClockConfigstring = CONFIG_LAST + 1;
static_assert(kClockConfigstring < CS_GENERAL + MAX_GENERAL, "no free general configstring");
std::string status_text; // what the configstrings hold now
std::string clock_text;

player_stat_t StatusStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("match_status");
	return (player_stat_t)(slot ? *slot : 0);
}

player_stat_t ClockStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("match_clock");
	return (player_stat_t)(slot ? *slot : 0);
}

// The HUD's big countdown digits: seconds left in the match or round countdown.
player_stat_t CountdownStat()
{
	static const std::optional<int> slot = Core_StatSlots().Allocate("match_countdown");
	return (player_stat_t)(slot ? *slot : 0);
}

void Publish(int configstring, std::string &published, const std::string &text)
{
	if (text == published)
		return;
	published = text;
	gi.configstring(configstring, text.c_str());
}

void PublishHud()
{
	// With the flow off, stock's timelimit on the clock: in deathmatch under a
	// registered gametype (Rogue's stock gamerules modes stay stock).
	MatchHudText hud;
	if (match.OwnsRules())
		hud = match.HudText(Snapshot());
	else if (deathmatch->integer && Core_Gametype_Active())
		hud = Core_Match_StockHudText(level.time.milliseconds(), (int64_t)(timelimit->value * 60000));
	// An open vote takes the status's place; the clock stays (core/vote_register.cpp).
	if (std::string vote = Core_Vote_HudText(); !vote.empty())
		hud.status = std::move(vote);
	Publish(kStatusConfigstring, status_text, hud.status);
	Publish(kClockConfigstring, clock_text, hud.clock);
}

// Warmup and the countdown are free play: every frag, suicide and capture is
// undone in the frame it happens, before any client sees it.
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
	r.ruleset = gi.cvar("g_ruleset", "", CVAR_SERVERINFO)->string;
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
		case MatchActionKind::Klaxon:
			// Heard everywhere, as stock CTF plays its match countdown.
			gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("world/klaxon2.wav"),
			                    1, ATTN_NONE, 0);
			break;
		case MatchActionKind::Sound:
			gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(a.text.c_str()), 1,
			                    ATTN_NONE, 0);
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
		status_text.clear(); // configstrings start empty on a new level
		clock_text.clear();
		if (match.OwnsRules())
			gi.Com_PrintFmt("Match: {}\n", MatchStateName(match.State()));
	}
	last_time = level.time;
	Core_Vote_Frame();
	Core_Maps_Frame();
	// Settings that change before the match starts take effect: an admin's
	// rulesets/<gametype>.cfg lands after the level has begun (the engine runs
	// queued commands when it gets to them), and so does a `set` in warmup.
	if (match.OwnsRules() && match.State() == MatchState::Warmup && !level.intermissiontime)
	{
		const MatchConfig config = ReadConfig();
		if (!(config == match.Config()))
		{
			match = Match(config);
			match.Begin(level.time.milliseconds());
			gi.Com_PrintFmt("Match: settings changed; {}\n", MatchStateName(match.State()));
		}
	}

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
		PublishHud();
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
	PublishHud();
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
	if (!deathmatch->integer || !StatusStat() || !ClockStat() || !CountdownStat())
		return;
	// Each in a place of its own (Chris, 2026-10-27), so either can show with
	// the other. The status top middle, in green like the clock: the row under the client's
	// 4 notify rows (10 units each from the top, scr_maxlines 4), above where
	// centre prints start (20% down, 48 units at the least); yt 43, not 40,
	// because the client draws this text about 2 units higher than the notify
	// font. The clock bottom left of centre, in green, just above the stock
	// HUD: centred halfway between the screen centre and the box's left edge
	// (xv 80, over the gap between the health icon and the ammo; cstring
	// centres on xv + 160).
	// Stock tokens only, and no `ifbit`.
	const player_stat_t status = StatusStat(), clock = ClockStat(), countdown = CountdownStat();
	sb.ifstat(status).xv(0).yt(43);
	sb.sb << "loc_stat_cstring2 " << status << ' ';
	sb.endifstat();
	sb.ifstat(clock).xv(-80).yb(-61);
	sb.sb << "loc_stat_cstring2 " << clock << ' ';
	sb.endifstat();
	// The countdown in big HUD digits, between the centre prints and the
	// crosshair. num 3 right-aligns in a 50 px field, so xv 118 centres one
	// digit; yv counts from the centred 240-high box, so it lands the same
	// distance above the crosshair at every resolution (yb -256 was off the
	// top of a 240-unit-high screen: 1440p, 4K).
	sb.ifstat(countdown).xv(118).yv(64).num(3, countdown).endifstat();
}

void Core_Match_SetStats(edict_t *ent)
{
	if (StatusStat())
		ent->client->ps.stats[StatusStat()] = status_text.empty() ? 0 : kStatusConfigstring;
	if (ClockStat())
		ent->client->ps.stats[ClockStat()] = clock_text.empty() ? 0 : kClockConfigstring;
	if (CountdownStat())
		ent->client->ps.stats[CountdownStat()] =
		    Core_Match_OwnsRules() ? match.CountdownSeconds(level.time.milliseconds()) : 0;
}

bool Core_Match_HoldFire(const edict_t *ent)
{
	return Core_Match_OwnsRules() && match.HoldFire() && Playing(ent);
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
	Core_Match_SetReady(ent, want_ready);
	return true;
}

bool Core_Match_Playing(const edict_t *ent)
{
	return Playing(ent);
}

bool Core_Match_ReadyUp()
{
	return Core_Match_OwnsRules() && match.Config().ready_required &&
	       (match.State() == MatchState::Warmup || match.State() == MatchState::Countdown);
}

bool Core_Match_Warmup()
{
	return Core_Match_OwnsRules() && match.State() == MatchState::Warmup;
}

bool Core_Match_InPlay()
{
	if (level.intermissiontime)
		return false;
	return !Core_Match_OwnsRules() || MatchStateInPlay(match.State());
}

bool Core_Match_IsReady(const edict_t *ent)
{
	return ready[(uint32_t)(ent - g_edicts) - 1];
}

void Core_Match_SetReady(edict_t *ent, bool want_ready)
{
	if (match.State() != MatchState::Warmup && match.State() != MatchState::Countdown)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "There is no match warmup.\n");
		return;
	}
	if (!Playing(ent))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Join the game first.\n");
		return;
	}
	const uint32_t slot = (uint32_t)(ent - g_edicts) - 1;
	if (ready[slot] != want_ready)
	{
		ready[slot] = want_ready;
		gi.LocBroadcast_Print(PRINT_HIGH, want_ready ? "{} is ready.\n" : "{} is not ready.\n",
		                      ent->client->pers.netname);
	}
}
