// Engine-facing part of the match flow (docs/design/match-flow.md): builds the
// state machine's input from the level each frame, carries out its actions,
// and handles the ready / notready commands.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match.h"
#include "core/match_record.h"
#include "mod_identity.h"
#include "core/match_hooks.h"

#include <array>
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
    *match_rounds, *match_round_countdown;

void RegisterCvars()
{
	if (match_warmup)
		return;
	match_warmup = gi.cvar("match_warmup", "0", CVAR_NOFLAGS);
	match_ready = gi.cvar("match_ready", "1", CVAR_NOFLAGS);
	match_minplayers = gi.cvar("match_minplayers", "2", CVAR_NOFLAGS);
	match_warmup_time = gi.cvar("match_warmup_time", "0", CVAR_NOFLAGS);
	match_countdown = gi.cvar("match_countdown", "5", CVAR_NOFLAGS);
	match_overtime = gi.cvar("match_overtime", "0", CVAR_NOFLAGS);
	match_rounds = gi.cvar("match_rounds", "0", CVAR_NOFLAGS);
	match_round_countdown = gi.cvar("match_round_countdown", "3", CVAR_NOFLAGS);
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
	c.timelimit_ms = (int64_t)(timelimit->value * 60000);
	c.fraglimit = fraglimit->integer;
	return c;
}

edict_t *ClientEdict(uint32_t i)
{
	return g_edicts + 1 + i;
}

// In the game as a player: begun (pers.spawned, set by ClientBegin) and not a
// spectator. A client that connected but never began - Q2REX's bots in a
// bot-only match - is not, so the arena reset never puts it in the world.
bool Playing(const edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->pers.connected && ent->client->pers.spawned &&
	       !ent->client->resp.spectator;
}

MatchInput Snapshot()
{
	MatchInput in;
	in.now_ms = level.time.milliseconds();
	const GametypeDef *gt = Core_Gametype_Active();
	in.gametype_over = gt && gt->match_over && gt->match_over();
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
		in.players.push_back(p);
	}
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

// Between rounds: every playing client out of play, unseen, unable to respawn
// until RoundStart. The same hold CTF's match start uses (CTFStartMatch).
void RoundHold()
{
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (!Playing(ent))
			continue;
		ent->svflags = SVF_NOCLIENT;
		ent->flags &= ~FL_GODMODE;
		ent->client->respawn_time = level.time + 3600_sec;
		ent->client->ps.pmove.pm_type = PM_DEAD;
		ent->deadflag = true;
		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->client->ps.gunindex = 0;
		ent->client->ps.gunskin = 0;
		gi.linkentity(ent);
	}
}

// A round begins: every playing client back in at a spawn point.
void RoundStart()
{
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		edict_t *ent = ClientEdict(i);
		if (!Playing(ent))
			continue;
		ent->client->respawn_time = level.time;
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
		r.players.push_back(
		    {ent->client->pers.netname, ent->client->resp.score, Playing(ent), (ent->svflags & SVF_BOT) != 0});
	}
	std::error_code ec;
	const std::filesystem::path dir(g_matchlog_dir->string);
	std::filesystem::create_directories(dir, ec);
	const std::filesystem::path file = dir / Core_MatchRecord_FileName(r.ended_at_utc, r.map);
	std::ofstream out(file, std::ios::binary);
	out << Core_MatchRecord_ToJson(r);
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
			CenterAll((match.State() == MatchState::RoundCountdown ? "Next round in " : "Match starts in ") +
			          std::to_string(a.value));
			break;
		case MatchActionKind::ResetArena:
			ResetArena(a.value == 0);
			break;
		case MatchActionKind::RoundHold:
			RoundHold();
			break;
		case MatchActionKind::RoundStart:
			RoundStart();
			break;
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
		return;

	const MatchState before = match.State();
	const std::vector<MatchAction> actions = match.Update(Snapshot());
	if (match.State() != before)
		gi.Com_PrintFmt("Match: {} -> {}\n", MatchStateName(before), MatchStateName(match.State()));
	Act(actions);
}

bool Core_Match_OwnsRules()
{
	return begun && match.OwnsRules();
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
	if (!Core_Match_OwnsRules() || (match.State() != MatchState::Warmup && match.State() != MatchState::Countdown))
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
