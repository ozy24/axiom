// Engine-facing part of voting (docs/design/voting.md): the commands, who
// counts, the HUD line, and carrying out a passed vote as console commands.
// The rules are core/vote.cpp.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match_hooks.h"
#include "core/player_name.h"
#include "core/rulesets.h"
#include "core/vote.h"
#include "core/vote_hooks.h"

#include <cstring>

namespace
{
Vote vote;
gtime_t last_time;
bool begun = false;

cvar_t *g_vote;
cvar_t *g_vote_allow;
cvar_t *g_vote_midgame;
cvar_t *g_vote_time;
cvar_t *g_vote_cooldown;
cvar_t *g_vote_limit;
cvar_t *g_map_list;

void RegisterCvars()
{
	if (g_vote)
		return;
	g_vote = gi.cvar("g_vote", "1", CVAR_NOFLAGS);
	g_vote_allow = gi.cvar("g_vote_allow", "map gametype ruleset restart", CVAR_NOFLAGS);
	g_vote_midgame = gi.cvar("g_vote_midgame", "1", CVAR_NOFLAGS);
	g_vote_time = gi.cvar("g_vote_time", "30", CVAR_NOFLAGS);
	g_vote_cooldown = gi.cvar("g_vote_cooldown", "30", CVAR_NOFLAGS);
	g_vote_limit = gi.cvar("g_vote_limit", "3", CVAR_NOFLAGS);
	g_map_list = gi.cvar("g_map_list", "", CVAR_NOFLAGS);
}

bool Active()
{
	return deathmatch->integer && Core_Gametype_Active();
}

VoteConfig ReadConfig()
{
	VoteConfig c;
	c.enabled = g_vote->integer != 0;
	for (const std::string &word : Core_Vote_Words(g_vote_allow->string))
		if (const auto kind = Core_Vote_ParseKind(word))
			c.allowed.insert(*kind);
	c.midgame = g_vote_midgame->integer != 0;
	c.timeout_ms = (int64_t)(std::max(5.f, g_vote_time->value) * 1000);
	c.fail_cooldown_ms = (int64_t)(std::max(0.f, g_vote_cooldown->value) * 1000);
	c.call_limit = std::max(0, g_vote_limit->integer);
	return c;
}

int Slot(const edict_t *ent)
{
	return (int)(ent - g_edicts) - 1;
}

const GametypeDef *GametypeById(std::string_view id)
{
	return Core_Gametype_Find(core_gametypes, core_gametype_count, id);
}

bool IsCtf(const GametypeDef *gt)
{
	return gt && !strcmp(gt->retail_mode, "ctf");
}

// Who counts: humans in the match; in Duel, everyone waiting in the queue too.
bool IsVoter(const edict_t *ent)
{
	if (!ent->inuse || !ent->client || !ent->client->pers.connected || !ent->client->pers.spawned ||
	    (ent->svflags & SVF_BOT))
		return false;
	const GametypeDef *gt = Core_Gametype_Active();
	return Core_Match_Playing(ent) || (gt && !strcmp(gt->id, "duel"));
}

std::vector<int> Voters()
{
	std::vector<int> out;
	for (uint32_t i = 1; i <= game.maxclients; ++i)
		if (IsVoter(g_edicts + i))
			out.push_back((int)i - 1);
	return out;
}

VoteChoices Choices()
{
	VoteChoices c;
	c.maps = Core_Vote_MapList(g_map_list->string, ctf->integer != 0);
	for (size_t i = 0; i < core_gametype_count; ++i)
		c.gametypes.push_back(core_gametypes[i]->id);
	for (size_t i = 0; i < core_ruleset_count; ++i)
		c.rulesets.push_back(core_rulesets[i].name);
	return c;
}

// A match is being played: votes then need g_vote_midgame. With the match
// flow off there is no warmup, so votes are always open.
bool Live()
{
	return Core_Match_OwnsRules() && !Core_Match_Warmup();
}

// The gametype a ruleset selects ("set g_gametype tdm" in its text), or null.
const GametypeDef *RulesetGametype(std::string_view name)
{
	const RulesetDef *def = Core_Ruleset_Find(name);
	if (!def)
		return nullptr;
	const char *line = strstr(def->cfg, "set g_gametype ");
	if (!line)
		return nullptr;
	line += strlen("set g_gametype ");
	return GametypeById(std::string_view(line, strcspn(line, "\n")));
}

// The console text that carries out a passed vote. Every kind ends in `map`,
// a fresh load of the level, so latched settings (g_gametype) take effect.
std::string Commands(const VoteProposal &p)
{
	const auto map_for = [](const GametypeDef *gt)
	{ return Core_Vote_MapFor(Core_Vote_MapList(g_map_list->string, IsCtf(gt)), level.mapname); };
	switch (p.kind)
	{
	case VoteKind::Map:
		return "map " + p.arg + "\n";
	case VoteKind::Restart:
		return std::string("map ") + level.mapname + "\n";
	case VoteKind::Gametype:
		return "set g_gametype " + p.arg + "\nmap " + map_for(GametypeById(p.arg)) + "\n";
	case VoteKind::Ruleset:
	{
		const GametypeDef *gt = RulesetGametype(p.arg);
		return Core_Ruleset_Commands(p.arg) + "map " + map_for(gt ? gt : Core_Gametype_Active()) + "\n";
	}
	}
	return {};
}

// Open menus redraw now, so their vote rows follow the vote.
void DirtyMenus()
{
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		edict_t *ent = g_edicts + i;
		if (ent->inuse && ent->client && ent->client->menu)
		{
			ent->client->menudirty = true;
			ent->client->menutime = level.time;
		}
	}
}

void Reply(edict_t *ent, VoteError e, const VoteProposal &p)
{
	const std::string text = Core_Vote_ErrorText(e, vote.Config(), p, Choices());
	if (!text.empty())
		gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", text.c_str());
}

void Call(edict_t *ent, std::string_view kind_name, std::string_view arg)
{
	const std::optional<VoteKind> kind = Core_Vote_ParseKind(kind_name);
	if (!kind)
	{
		std::string kinds;
		for (VoteKind k : vote.Config().allowed)
			kinds += std::string(kinds.empty() ? "" : " ") + VoteKindName(k);
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: callvote <{}> [choice]\n",
		                   kinds.empty() ? "nothing" : kinds.c_str());
		return;
	}
	if (level.intermissiontime)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Wait for the next map.\n");
		return;
	}
	const VoteProposal p{*kind, std::string(arg)};
	const VoteError e = vote.Call(Slot(ent), p, Choices(), Live(), Voters(), level.time.milliseconds());
	if (e != VoteError::None)
	{
		Reply(ent, e, p);
		return;
	}
	gi.LocBroadcast_Print(PRINT_HIGH, "{} called a vote: {}. Vote with `vote yes` / `vote no` or the Tab menu.\n",
	                      ent->client->pers.netname, Core_Vote_Describe(vote.Proposal()).c_str());
	gi.Com_PrintFmt("Vote: {} called {}\n", Core_PlayerName(ent), Core_Vote_Describe(vote.Proposal()));
	DirtyMenus();
}

void Cast(edict_t *ent, bool yes)
{
	const VoteError e = vote.Cast(Slot(ent), yes, Voters());
	if (e != VoteError::None)
	{
		Reply(ent, e, vote.Proposal());
		return;
	}
	gi.LocClient_Print(ent, PRINT_HIGH, yes ? "You voted yes.\n" : "You voted no.\n");
	DirtyMenus();
}
} // namespace

void Core_Vote_Frame()
{
	if (!Active())
		return;
	RegisterCvars();
	// A new level: the open vote and the call limits are gone.
	if (!begun || level.time < last_time)
	{
		vote.Reset();
		begun = true;
	}
	last_time = level.time;
	vote.SetConfig(ReadConfig());

	const VoteProposal p = vote.Proposal();
	const std::string what = Core_Vote_Describe(p);
	switch (vote.Update(Voters(), level.time.milliseconds()))
	{
	case VoteEvent::None:
		return;
	case VoteEvent::Passed:
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote passed: {}.\n", what.c_str());
		gi.Com_PrintFmt("Vote: {} passed\n", what);
		break;
	case VoteEvent::Failed:
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote failed: {}.\n", what.c_str());
		gi.Com_PrintFmt("Vote: {} failed\n", what);
		break;
	case VoteEvent::TimedOut:
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote timed out: {}.\n", what.c_str());
		gi.Com_PrintFmt("Vote: {} timed out\n", what);
		break;
	case VoteEvent::Execute:
	{
		const std::string commands = Commands(p);
		gi.Com_PrintFmt("Vote: running {}", commands);
		gi.AddCommandString(commands.c_str());
		break;
	}
	}
	DirtyMenus();
}

bool Core_Vote_ClientCommand(edict_t *ent, const char *cmd)
{
	if (!Active())
		return false;
	RegisterCvars();
	if (!Q_strcasecmp(cmd, "callvote") || !Q_strcasecmp(cmd, "warp"))
	{
		const bool warp = !Q_strcasecmp(cmd, "warp"); // stock's map election: the same as `callvote map`
		const int first = warp ? 1 : 2;
		std::string arg;
		for (int i = first; i < gi.argc(); ++i)
			arg += std::string(arg.empty() ? "" : " ") + gi.argv(i);
		Call(ent, warp ? "map" : (gi.argc() > 1 ? gi.argv(1) : ""), arg);
		return true;
	}
	if (!Q_strcasecmp(cmd, "vote"))
	{
		const char *answer = gi.argv(1);
		const bool yes = !Q_strcasecmp(answer, "yes") || !strcmp(answer, "1");
		if (!yes && Q_strcasecmp(answer, "no") && strcmp(answer, "0"))
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: vote yes|no\n");
			return true;
		}
		Cast(ent, yes);
		return true;
	}
	// Stock's `yes` / `no` answer our vote while one is open; otherwise they
	// stay stock's (CTF's own elections).
	const bool yes = !Q_strcasecmp(cmd, "yes");
	if ((yes || !Q_strcasecmp(cmd, "no")) && vote.Open())
	{
		Cast(ent, yes);
		return true;
	}
	return false;
}

void Core_Vote_Disconnect(edict_t *ent)
{
	vote.Forget(Slot(ent));
}

std::string Core_Vote_HudText()
{
	return Active() ? vote.HudText() : std::string();
}

VoteMenuInfo Core_Vote_MenuInfo(const edict_t *ent)
{
	VoteMenuInfo info;
	if (!Active())
		return info;
	RegisterCvars();
	const VoteConfig &config = vote.Config();
	info.voter = IsVoter(ent);
	info.open = vote.Open();
	info.proposal = info.open ? Core_Vote_Describe(vote.Proposal()) : "";
	if (const std::optional<bool> a = vote.Answer(Slot(ent)))
		info.answer = *a ? 1 : 0;
	info.can_call = config.enabled && !config.allowed.empty() && info.voter && !vote.Open() && !vote.Passed() &&
	                !level.intermissiontime && (!Live() || config.midgame);
	for (VoteKind k : config.allowed)
		info.kinds.push_back(VoteKindName(k));
	const VoteChoices choices = Choices();
	info.maps = choices.maps;
	for (size_t i = 0; i < core_gametype_count; ++i)
		info.gametypes.emplace_back(core_gametypes[i]->id, core_gametypes[i]->title);
	info.rulesets = choices.rulesets;
	const GametypeDef *gt = Core_Gametype_Active();
	info.gametype = gt ? gt->id : "";
	info.ruleset = gi.cvar("g_ruleset", "stock", CVAR_SERVERINFO)->string;
	return info;
}

void Core_Vote_Call(edict_t *ent, const std::string &args)
{
	RegisterCvars();
	const size_t space = args.find(' ');
	Call(ent, std::string_view(args).substr(0, space),
	     space == std::string::npos ? std::string_view() : std::string_view(args).substr(space + 1));
}

void Core_Vote_Cast(edict_t *ent, bool yes)
{
	RegisterCvars();
	Cast(ent, yes);
}
