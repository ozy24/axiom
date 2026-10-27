// Engine-facing part of the Tab menu (docs/design/menu.md). The pages come
// from core/menu.cpp; this file turns one into stock's pmenu entries, so
// stock's code does the rest: drawing, the cursor (invnext / invprev, and
// forward / back for a free spectator), selecting (invuse, fire, jump),
// putaway, closing for the scoreboard, and the 3-second refresh.
#include "g_local.h"

#include "core/gametype.h"
#include "core/match_hooks.h"
#include "core/menu.h"
#include "core/menu_hooks.h"
#include "core/vote_hooks.h"

#include <cstring>

// id's, with no declaration in a header.
void spectator_respawn(edict_t *ent);
void CTFJoinTeam(edict_t *ent, ctfteam_t desired_team);
void CTFChaseCam(edict_t *ent, pmenuhnd_t *p);

namespace
{
enum class Page : int32_t
{
	Main,
	MatchInfo,
	CallVote,
	VoteMaps,
	VoteGametypes,
	VoteRulesets,
};

// The menu's arg (PMenu_Open TagFrees it on close): which page, and what each
// drawn row does. Rebuilt with the rows on every refresh, so a row and its
// action always match.
struct MenuState
{
	Page page;
	MenuAction actions[kMenuRows];
	char args[kMenuRows][64]; // VoteFor: "map q2dm3"
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
	v.gametype = gt ? gt->title : "";
	v.map = *level.level_name ? level.level_name : level.mapname;
	v.map_file = level.mapname;
	v.ruleset = gi.cvar("g_ruleset", "stock", CVAR_SERVERINFO)->string;
	v.teams = G_TeamplayEnabled();
	v.ctf = ctf->integer != 0;
	v.duel = gt && !strcmp(gt->id, "duel");

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
	v.rounds = CvarInt("match_rounds") != 0;
	v.match_flow = Core_Match_OwnsRules();

	const VoteMenuInfo vote = Core_Vote_MenuInfo(ent);
	v.voter = vote.voter;
	v.vote_open = vote.open;
	v.vote_proposal = vote.proposal;
	v.vote_answer = vote.answer;
	v.vote_can_call = vote.can_call;
	v.vote_kinds = vote.kinds;
	v.vote_maps = vote.maps;
	v.vote_gametypes = vote.gametypes;
	v.vote_rulesets = vote.rulesets;
	v.gametype_id = vote.gametype;
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

MenuPage Build(const edict_t *ent, Page page)
{
	const MenuView view = View(ent);
	switch (page)
	{
	case Page::Main:
		return Core_Menu_Main(view);
	case Page::MatchInfo:
		return Core_Menu_MatchInfo(view);
	case Page::CallVote:
		return Core_Menu_CallVote(view);
	case Page::VoteMaps:
		return Core_Menu_VoteChoices(view, "map");
	case Page::VoteGametypes:
		return Core_Menu_VoteChoices(view, "gametype");
	case Page::VoteRulesets:
		return Core_Menu_VoteChoices(view, "ruleset");
	}
	return {};
}

// UpdateFunc: stock calls it before each redraw (a cursor move, a dirty menu,
// the 3-second refresh). The cursor stays on its row while that row still
// does something.
void Update(edict_t *ent)
{
	pmenuhnd_t *hnd = ent->client->menu;
	MenuState *state = (MenuState *)hnd->arg;
	const MenuPage page = Build(ent, state->page);
	Fill(hnd->entries, *state, page);
	if (hnd->cur < 0 || hnd->cur >= kMenuRows || state->actions[hnd->cur] == MenuAction::None)
		hnd->cur = Core_Menu_Next(page, -1);
}

void Open(edict_t *ent, Page which)
{
	if (ent->client->menu)
		PMenu_Close(ent);
	MenuState *state = (MenuState *)gi.TagMalloc(sizeof(MenuState), TAG_LEVEL);
	state->page = which;
	const MenuPage page = Build(ent, which);
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

// FFA: the stock spectator toggle, as the `spectator` userinfo cvar drives it;
// the client's cvar is set to match, so its next userinfo agrees.
void SetSpectator(edict_t *ent, bool spectator)
{
	ent->client->pers.spectator = spectator;
	spectator_respawn(ent);
	gi.WriteByte(svc_stufftext);
	gi.WriteString(spectator ? "spectator 1\n" : "spectator 0\n");
	gi.unicast(ent, true);
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
		CTFJoinTeam(ent, team); // as stock's join menu; closes the menu
		break;
	}
	case MenuAction::JoinGame:
		PMenu_Close(ent);
		SetSpectator(ent, false);
		break;
	case MenuAction::Spectate:
		PMenu_Close(ent);
		if (G_TeamplayEnabled())
		{
			CTFObserver(ent); // `observer`; its respawn reopens the join menu
			PMenu_Close(ent);
		}
		else
			SetSpectator(ent, true);
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
	case MenuAction::Queue:
		PMenu_Close(ent);
		if (const GametypeDef *gt = Core_Gametype_Active(); gt && gt->client_command)
			gt->client_command(ent, "queue");
		break;
	case MenuAction::MatchInfo:
		Open(ent, Page::MatchInfo);
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
	case MenuAction::VoteRulesets:
		Open(ent, Page::VoteRulesets);
		break;
	case MenuAction::VoteFor:
	{
		const std::string args = state->args[hnd->cur]; // the menu's arg goes on close
		PMenu_Close(ent);
		Core_Vote_Call(ent, args);
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
