// Voting's hooks into id's code (each a marked one-line call) and what the
// Tab menu and the HUD read from it.
#pragma once

#include <string>
#include <utility>
#include <vector>

struct edict_t;

// Every server frame in deathmatch, from Core_Match_Frame: counts the open
// vote, announces the result and runs a passed one.
void Core_Vote_Frame();

// `callvote`, `vote`, and while a vote is open stock's `yes` / `no`; `warp`
// becomes `callvote map`. True if handled. From ClientCommand, before id's
// chain. Off outside deathmatch and under Rogue's stock gamerules modes.
bool Core_Vote_ClientCommand(edict_t *ent, const char *cmd);

// From ClientDisconnect: the slot's answer goes, so its next client starts
// without one.
void Core_Vote_Disconnect(edict_t *ent);

// The HUD status line while a vote is open or passed ("VOTE: MAP Q2DM3
// (1/2)"), else empty: it takes the line from the match status.
std::string Core_Vote_HudText();

// For the Tab menu (core/menu_register.cpp).
struct VoteMenuInfo
{
	bool voter = false;             // counts in a vote now
	bool open = false;              // a vote is open
	std::string proposal;           // "map q2dm3"
	int answer = -1;                // this client's: -1 none, 0 no, 1 yes
	bool can_call = false;          // voting on, a voter, nothing open, not a live match (or midgame)
	std::vector<std::string> kinds; // allowed: "map", "gametype", "ruleset", "restart"
	std::vector<std::string> maps;
	std::vector<std::pair<std::string, std::string>> gametypes; // (id, title)
	std::vector<std::string> rulesets;
	std::string gametype; // the current one's id
	std::string ruleset;  // g_ruleset
};
VoteMenuInfo Core_Vote_MenuInfo(const edict_t *ent);

// What the menu's rows do: `callvote <args>` and `vote yes|no`, through the
// same checks and messages as the console.
void Core_Vote_Call(edict_t *ent, const std::string &args);
void Core_Vote_Cast(edict_t *ent, bool yes);
