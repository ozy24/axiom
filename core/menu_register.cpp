// Engine-facing part of the Tab menu (docs/design/menu.md). The pages come
// from core/menu.cpp; this file turns one into stock's pmenu entries, so
// stock's code does the rest: drawing, the cursor (invnext / invprev, and
// forward / back for a free spectator), selecting (invuse, fire, jump),
// putaway, closing for the scoreboard, and the 3-second refresh.
#include "g_local.h"
#include "mod_identity.h"

#include "core/gametype.h"
#include "core/maps_hooks.h"
#include "core/match_hooks.h"
#include "core/menu.h"
#include "core/join_hooks.h"
#include "core/menu_hooks.h"
#include "core/teams_hooks.h"
#include "core/vote_hooks.h"

#include <cstring>

// id's, with no declaration in a header.
void CTFJoinTeam(edict_t *ent, ctfteam_t desired_team);
void CTFChaseCam(edict_t *ent, pmenuhnd_t *p);

namespace
{
enum class Page : int32_t
{
	Main,
	MatchInfo,
	About,
	CallVote,
	VoteMaps,
	VoteGametypes,
	VoteVariants,
	VoteSetting,
	Team,
	TeamMate,
};

// The menu's arg (PMenu_Open TagFrees it on close): which page, and what each
// drawn row does. Rebuilt with the rows on every refresh, so a row and its
// action always match.
struct MenuState
{
	Page page;
	MenuAction actions[kMenuRows];
	char args[kMenuRows][64]; // VoteFor: "map q2dm3"; VoteVariants: the gametype; VoteSetting: the kind
	char gametype[32];        // Page::VoteVariants: whose gametype; Page::VoteSetting: which setting;
	                          // Page::TeamMate: the teammate's client number
};

bool Active()
{
	return deathmatch->integer && Core_Gametype_Active();
}

int CvarInt(const char *name)
{
	const cvar_t *c = gi.cvar(name, "0", CVAR_NOFLAGS);
	return c ? c->integer : 0;
}

MenuView View(const edict_t *ent)
{
	MenuView v;
	const GametypeDef *gt = Core_Gametype_Active();
	v.hostname = gi.cvar("hostname", "", CVAR_NOFLAGS)->string;
	v.mod_name = MOD_NAME;
	v.mod_version = MOD_VERSION;
	v.mod_build = MOD_BUILD;
	v.mod_page = MOD_REPOSITORY;
	v.gametype = gt ? gt->title : "";
	v.map = *level.level_name ? level.level_name : level.mapname;
	v.map_file = level.mapname;
	v.next_map = Core_Maps_NextMap();
	v.ruleset = gi.cvar("g_ruleset", "", CVAR_SERVERINFO)->string;
	v.teams = G_TeamplayEnabled();
	v.ctf = ctf->integer != 0;
	v.duel = gt && !strcmp(gt->id, "duel");
	v.queued = gt && gt->waiting && gt->waiting(ent);

	v.playing = Core_Match_Playing(ent);
	v.chasing = ent->client->chase_target != nullptr;
	v.team = ent->client->resp.ctf_team == CTF_TEAM1 ? 1 : ent->client->resp.ctf_team == CTF_TEAM2 ? 2 : 0;
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		const edict_t *other = g_edicts + i;
		if (!other->inuse || !other->client || !Core_Match_Playing(other))
			continue;
		v.red += other->client->resp.ctf_team == CTF_TEAM1;
		v.blue += other->client->resp.ctf_team == CTF_TEAM2;
	}

	v.ready_up = Core_Match_ReadyUp();
	v.ready = Core_Match_IsReady(ent);
	v.ready_required = CvarInt("match_ready") != 0;
	v.timelimit_min = (int)timelimit->value;
	v.fraglimit = fraglimit->integer;
	v.capturelimit = capturelimit->integer;
	v.overtime = CvarInt("match_overtime");
	v.match_flow = Core_Match_OwnsRules();
	v.warmup = v.match_flow && !Core_Match_InPlay();

	const VoteMenuInfo vote = Core_Vote_MenuInfo(ent);
	v.voter = vote.voter;
	v.vote_open = vote.open;
	v.vote_proposal = vote.proposal;
	v.vote_answer = vote.answer;
	v.vote_can_call = vote.can_call;
	v.vote_kinds = vote.kinds;
	v.vote_maps = vote.maps;
	v.vote_gametypes = vote.gametypes;
	v.vote_settings = vote.settings;
	v.gametype_id = vote.gametype;
	Core_Teams_MenuInfo(ent, v);
	return v;
}

void Select(edict_t *ent, pmenuhnd_t *hnd);

// The page's rows into pmenu entries: stock marks a highlighted row with a
// leading '*'; a row with an action gets Select.
void Fill(pmenu_t *entries, MenuState &state, const MenuPage &page)
{
	for (int i = 0; i < kMenuRows; ++i)
	{
		const MenuRow row = i < (int)page.size() ? page[i] : MenuRow{};
		const std::string text = row.highlight ? "*" + row.text : row.text;
		Q_strlcpy(entries[i].text, text.c_str(), sizeof(entries[i].text));
		entries[i].align = row.align == MenuAlign::Center  ? PMENU_ALIGN_CENTER
		                   : row.align == MenuAlign::Right ? PMENU_ALIGN_RIGHT
		                                                   : PMENU_ALIGN_LEFT;
		entries[i].SelectFunc = row.action != MenuAction::None ? Select : nullptr;
		entries[i].text_arg1[0] = '\0';
		state.actions[i] = row.action;
		Q_strlcpy(state.args[i], row.arg.c_str(), sizeof(state.args[i]));
	}
}

MenuPage Build(const edict_t *ent, Page page, const char *gametype)
{
	const MenuView view = View(ent);
	switch (page)
	{
	case Page::Main:
		return Core_Menu_Main(view);
	case Page::MatchInfo:
		return Core_Menu_MatchInfo(view);
	case Page::About:
		return Core_Menu_About(view);
	case Page::CallVote:
		return Core_Menu_CallVote(view);
	case Page::VoteMaps:
		return Core_Menu_VoteMaps(view);
	case Page::VoteGametypes:
		return Core_Menu_VoteGametypes(view);
	case Page::VoteVariants:
		return Core_Menu_VoteVariants(view, gametype);
	case Page::VoteSetting:
		return Core_Menu_VoteSetting(view, gametype);
	case Page::Team:
		return Core_Menu_Team(view);
	case Page::TeamMate:
		return Core_Menu_TeamMate(view, atoi(gametype));
	}
	return {};
}

// UpdateFunc: stock calls it before each redraw (a cursor move, a dirty menu,
// the 3-second refresh). The cursor follows what its row did, as rows move
// when a vote opens or closes: a press must never do something else.
void Update(edict_t *ent)
{
	pmenuhnd_t *hnd = ent->client->menu;
	MenuState *state = (MenuState *)hnd->arg;
	const bool on_row = hnd->cur >= 0 && hnd->cur < kMenuRows;
	const MenuAction was = on_row ? state->actions[hnd->cur] : MenuAction::None;
	const std::string arg = on_row ? state->args[hnd->cur] : "";
	const MenuPage page = Build(ent, state->page, state->gametype);
	Fill(hnd->entries, *state, page);
	hnd->cur = Core_Menu_Keep(page, hnd->cur, was, arg);
}

void Open(edict_t *ent, Page which, const char *gametype = "")
{
	if (ent->client->menu)
		PMenu_Close(ent);
	MenuState *state = (MenuState *)gi.TagMalloc(sizeof(MenuState), TAG_LEVEL);
	state->page = which;
	Q_strlcpy(state->gametype, gametype, sizeof(state->gametype));
	const MenuPage page = Build(ent, which, gametype);
	pmenu_t entries[kMenuRows] = {};
	Fill(entries, *state, page);
	const int cur = which == Page::Main ? Core_Menu_MainCursor(page, View(ent)) : Core_Menu_Next(page, -1);
	PMenu_Open(ent, entries, cur, kMenuRows, state, Update);
}

void StopChasing(edict_t *ent)
{
	ent->client->chase_target = nullptr;
	ent->client->ps.pmove.pm_flags &= ~(PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION);
}

void Select(edict_t *ent, pmenuhnd_t *hnd)
{
	const MenuState *state = (const MenuState *)hnd->arg;
	if (hnd->cur < 0 || hnd->cur >= kMenuRows)
		return;
	switch (state->actions[hnd->cur])
	{
	case MenuAction::None:
		break;
	case MenuAction::JoinRed:
	case MenuAction::JoinBlue:
	{
		const ctfteam_t team = state->actions[hnd->cur] == MenuAction::JoinRed ? CTF_TEAM1 : CTF_TEAM2;
		if (ent->client->resp.ctf_team == team)
		{
			PMenu_Close(ent);
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_already_on_team", CTFTeamName(team)); // as `team`
			break;
		}
		if (ent->client->resp.ctf_team == CTF_NOTEAM)
		{
			CTFJoinTeam(ent, team); // as stock's join menu; closes the menu
			break;
		}
		// A change of team is stock's `team` command's switch, run here: the
		// KEX client doesn't run a stuffed `team blue` (playtest 2026-10-29).
		// It kills the player (dropping a carried flag and tech), and refuses
		// under g_teamplay_force_join, a stock CTF match or a lock, as `team` does.
		PMenu_Close(ent);
		CTFChangeTeam(ent, team);
		break;
	}
	case MenuAction::JoinGame: // `join`: FFA into play, Duel into the queue
		PMenu_Close(ent);
		Core_Join_Run(ent, true);
		break;
	case MenuAction::Spectate: // `spectate`
		PMenu_Close(ent);
		Core_Join_Run(ent, false);
		PMenu_Close(ent); // team modes: `observer`'s respawn reopens the join menu
		break;
	case MenuAction::Chase:
		if (G_TeamplayEnabled())
		{
			CTFChaseCam(ent, hnd); // stock's: closes the menu, or says there's nobody
			break;
		}
		PMenu_Close(ent);
		GetChaseTarget(ent);
		break;
	case MenuAction::StopChase:
		PMenu_Close(ent);
		StopChasing(ent);
		break;
	case MenuAction::Ready:
	case MenuAction::NotReady:
		Core_Match_SetReady(ent, state->actions[hnd->cur] == MenuAction::Ready);
		PMenu_Update(ent); // the row flips
		break;
	case MenuAction::MatchInfo:
		Open(ent, Page::MatchInfo);
		break;
	case MenuAction::About:
		Open(ent, Page::About);
		break;
	case MenuAction::Back:
		Open(ent, Page::Main);
		break;
	case MenuAction::Close:
		PMenu_Close(ent);
		ent->client->update_chase = true;
		break;
	case MenuAction::VoteYes:
	case MenuAction::VoteNo:
	{
		const bool yes = state->actions[hnd->cur] == MenuAction::VoteYes;
		PMenu_Close(ent);
		Core_Vote_Cast(ent, yes);
		break;
	}
	case MenuAction::CallVote:
		Open(ent, Page::CallVote);
		break;
	case MenuAction::VoteMaps:
		Open(ent, Page::VoteMaps);
		break;
	case MenuAction::VoteGametypes:
		Open(ent, Page::VoteGametypes);
		break;
	case MenuAction::VoteVariants:
	{
		const std::string gametype = state->args[hnd->cur]; // Open frees this menu's state
		Open(ent, Page::VoteVariants, gametype.c_str());
		break;
	}
	case MenuAction::VoteSetting:
	{
		const std::string kind = state->args[hnd->cur]; // Open frees this menu's state
		Open(ent, Page::VoteSetting, kind.c_str());
		break;
	}
	case MenuAction::VoteFor:
	{
		const std::string args = state->args[hnd->cur]; // the menu's arg goes on close
		PMenu_Close(ent);
		Core_Vote_Call(ent, args);
		break;
	}
	case MenuAction::Team:
		Open(ent, Page::Team);
		break;
	case MenuAction::TeamLock:
	case MenuAction::TeamUnlock:
	case MenuAction::TeamReady:
	{
		const MenuAction action = state->actions[hnd->cur];
		Core_Teams_Run(ent,
		               action == MenuAction::TeamLock     ? "lockteam"
		               : action == MenuAction::TeamUnlock ? "unlockteam"
		                                                  : "readyteam",
		               nullptr);
		PMenu_Update(ent); // Lock flips to Unlock
		break;
	}
	case MenuAction::TeamMate:
	{
		const std::string client = state->args[hnd->cur]; // Open frees this menu's state
		Open(ent, Page::TeamMate, client.c_str());
		break;
	}
	case MenuAction::TeamKick:
	case MenuAction::TeamCaptain:
	{
		const bool kick = state->actions[hnd->cur] == MenuAction::TeamKick;
		const uint32_t client = (uint32_t)atoi(state->args[hnd->cur]);
		PMenu_Close(ent);
		if (client < game.maxclients)
			Core_Teams_Run(ent, kick ? "teamkick" : "captain", g_edicts + 1 + client);
		break;
	}
	}
}
} // namespace

bool Core_Menu_Inven(edict_t *ent)
{
	if (!Active())
		return false;
	Open(ent, Page::Main);
	return true;
}

bool Core_Menu_JoinMenu(edict_t *ent)
{
	return Core_Menu_Inven(ent);
}
