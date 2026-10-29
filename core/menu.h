// The Tab menu's pages (docs/design/menu.md): built from a snapshot of the
// game, drawn by stock's pmenu (rerelease/ctf/p_ctf_menu.cpp). Pure: no
// engine access. The adapter is core/menu_register.cpp.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// What choosing a row does. Each runs an existing command's code: the menu
// adds no game logic of its own.
enum class MenuAction
{
	None, // a text row: the cursor skips it
	JoinRed,
	JoinBlue,
	JoinGame,  // `join`: FFA, a spectator plays; Duel, joins the queue
	Spectate,  // `spectate`: a player stops playing; Duel, leaves the queue
	Chase,     // a spectator follows a player
	StopChase, // ... and stops
	Ready,
	NotReady,
	MatchInfo,
	About, // the mod's name, version and page
	Back,  // to the main page
	Close,
	VoteYes, // the open vote (core/vote.h)
	VoteNo,
	CallVote,      // the Call a Vote page; from a choice list, back to it
	VoteMaps,      // the maps
	VoteGametypes, // the gametypes (also Back from a gametype's variants)
	VoteVariants,  // a gametype's variants; the row's arg is its id
	VoteSetting,   // a setting's choices; the row's arg is its kind ("timelimit")
	VoteFor,       // callvote with the row's arg ("map q2dm3", "gametype hoony", "restart")
	Team,          // a captain's Team page (also Back from a teammate's page)
	TeamLock,      // lockteam
	TeamUnlock,    // unlockteam
	TeamReady,     // readyteam
	TeamMate,      // a teammate's page; the row's arg is their client number
	TeamKick,      // teamkick the row's arg (a client number)
	TeamCaptain,   // captain the row's arg (a client number)
};

enum class MenuAlign
{
	Left,
	Center,
	Right
};

struct MenuRow
{
	std::string text; // as drawn; a stock "$key" is localised by the client
	MenuAlign align = MenuAlign::Left;
	MenuAction action = MenuAction::None;
	bool highlight = false; // drawn in the alternate colour
	std::string arg;        // VoteFor: what to call; VoteVariants: the gametype; VoteSetting: the kind
};

using MenuPage = std::vector<MenuRow>;

// Stock's pmenu draws rows at yv 32 + i*8 under the inventory picture: 18
// fit. A left row starts at xv 64, so 28 characters stay inside the 320 px
// HUD box.
constexpr int kMenuRows = 18;
constexpr size_t kMenuLineChars = 28;
// The client keeps a layout in a MAX_STRING_CHARS buffer, NUL included.
constexpr size_t kMenuLayoutBytes = 1024;

// Text from players or the server (names, hostname, map titles) made safe
// for a pmenu row: control bytes dropped; `"` and `\` (which end or escape
// the layout's quoted string) and `{` `}` (format braces in the row's
// loc_string) replaced; a leading `*` (stock's highlight marker) or `$` (a
// localisation key) dropped; clipped to kMenuLineChars with "...".
std::string Core_Menu_Clean(std::string_view text);

// The layout stock's PMenu_Do_Update writes for a page with the cursor on
// row `cur` (-1 for none), byte for byte, so tests can hold every page to
// kMenuLayoutBytes.
std::string Core_Menu_Layout(const MenuPage &page, int cur);

// The cursor: the selectable row after / before `cur`, wrapping, as stock's
// PMenu_Next / PMenu_Prev move it; -1 if no row is selectable.
int Core_Menu_Next(const MenuPage &page, int cur);
int Core_Menu_Prev(const MenuPage &page, int cur);
// Where the cursor goes when the page is rebuilt under it (a vote opens,
// a row flips): the row that does what it was on (`action` with `arg`), as
// rows can move; else the same row if it still does something (Ready became
// Not Ready); else the first choice.
int Core_Menu_Keep(const MenuPage &page, int cur, MenuAction action, std::string_view arg);

// A gametype in the vote: its rulesets, the standard one first. One ruleset:
// voted from the list; more: a submenu of variants.
struct MenuVoteGametype
{
	std::string id;                                            // "duel"
	std::string title;                                         // "Duel"
	std::vector<std::pair<std::string, std::string>> rulesets; // (name, row text): ("hoony", "HoonyMode")
};

// A setting players can vote on (core/vote.h, decision 0015): its menu
// choices and the value now.
struct MenuVoteSetting
{
	std::string kind;                                         // "timelimit"
	std::string title;                                        // "Time Limit"
	std::vector<std::pair<std::string, std::string>> choices; // (arg, row text): ("20", "20 minutes")
	std::string current;                                      // the arg it has now: "10"
};

// What the pages show: the adapter fills it from the level each refresh.
struct MenuView
{
	std::string hostname;
	std::string gametype; // its title: "Free For All"
	std::string map;      // the level's title, else its file name
	std::string map_file; // "q2dm1"
	std::string next_map; // where the level goes next (core/maps_hooks.h)
	std::string ruleset;

	bool teams = false; // TDM, CTF
	bool ctf = false;
	bool duel = false;

	bool playing = false;  // in the match (on a team, in FFA, duelling)
	bool queued = false;   // Duel: asked to play (duelling or waiting)
	bool chasing = false;  // a spectator following someone
	int team = 0;          // teams: 0 none, 1 red, 2 blue
	int red = 0, blue = 0; // teams: players on each (the cursor's default)

	bool ready_up = false; // warmup or countdown with ready-up: show Ready / Not Ready
	bool ready = false;
	bool ready_required = false; // match_ready

	int timelimit_min = 0;
	int fraglimit = 0;
	int capturelimit = 0;
	int overtime = 0; // match_overtime: minutes, -1 sudden death, 0 none
	bool rounds = false;
	bool match_flow = false; // match_warmup: warmup, countdown and the HUD
	bool warmup = false;     // the match flow, before LIVE (warmup or the countdown)

	// Voting (core/vote.h).
	bool voter = false;                  // counts in a vote now
	bool vote_open = false;              // a vote is open
	std::string vote_proposal;           // "map q2dm3"
	int vote_answer = -1;                // this player's: -1 none, 0 no, 1 yes
	bool vote_can_call = false;          // Call a Vote shows
	std::vector<std::string> vote_kinds; // allowed: "map", "gametype", "restart"
	std::vector<std::string> vote_maps;
	std::vector<MenuVoteGametype> vote_gametypes;
	std::vector<MenuVoteSetting> vote_settings; // allowed, and for this gametype
	std::string gametype_id;                    // the current one, marked in its list

	// Team locks and captains (core/teams.h); [1] red, [2] blue.
	bool team_locked[3] = {};
	bool team_may_join[3] = {true, true, true};         // unlocked, or on its roster
	std::string team_captain[3];                        // the captain's name; empty: none
	bool captain = false;                               // this player captains their team
	std::vector<std::pair<int, std::string>> teammates; // a captain's: (client number, name), humans but them

	// About (mod.json, through mod_identity.h).
	std::string mod_name;    // MOD_NAME
	std::string mod_version; // "0.1.0"
	std::string mod_build;   // the commit: "452d70fb"
	std::string mod_page;    // MOD_REPOSITORY: the project's page; may be empty
};

// The main page, and the row its cursor starts on (team modes: the team
// with fewer players, else the first choice).
MenuPage Core_Menu_Main(const MenuView &view);
int Core_Menu_MainCursor(const MenuPage &page, const MenuView &view);

// The settings the match runs under.
MenuPage Core_Menu_MatchInfo(const MenuView &view);

// About: the mod's name, version, build and page.
MenuPage Core_Menu_About(const MenuView &view);

// Call a Vote: the allowed kinds; restart is called from here, the others
// open their choice list.
MenuPage Core_Menu_CallVote(const MenuView &view);
// The maps, the current one marked and skipped. A list longer than the page
// ends in a row saying to use callvote.
MenuPage Core_Menu_VoteMaps(const MenuView &view);
// The gametypes: one with a single ruleset is voted from here (marked and
// skipped if current); one with variants opens them (marked if current).
MenuPage Core_Menu_VoteGametypes(const MenuView &view);
// A gametype's variants ("Duel", "HoonyMode"), the current ruleset marked and
// skipped; Back returns to the gametypes.
MenuPage Core_Menu_VoteVariants(const MenuView &view, std::string_view gametype);
// A setting's choices ("5 minutes" ... "30 minutes"), the value now marked and
// skipped; Back returns to Call a Vote.
MenuPage Core_Menu_VoteSetting(const MenuView &view, std::string_view kind);

// A captain's Team page: lock or unlock, ready the team (in ready-up), and
// their teammates, each opening a teammate's page. Back to the main page.
MenuPage Core_Menu_Team(const MenuView &view);
// A teammate's page (`client` their number): make captain, kick; Back to Team.
MenuPage Core_Menu_TeamMate(const MenuView &view, int client);
