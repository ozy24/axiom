// The Tab menu's pages (docs/design/menu.md): built from a snapshot of the
// game, drawn by stock's pmenu (rerelease/ctf/p_ctf_menu.cpp). Pure: no
// engine access. The adapter is core/menu_register.cpp.
#pragma once

#include <string>
#include <string_view>
#include <vector>

// What choosing a row does. Each runs an existing command's code: the menu
// adds no game logic of its own.
enum class MenuAction
{
	None, // a text row: the cursor skips it
	JoinRed,
	JoinBlue,
	JoinGame,  // FFA: a spectator joins
	Spectate,  // FFA, TDM, CTF: a player stops playing
	Chase,     // a spectator follows a player
	StopChase, // ... and stops
	Ready,
	NotReady,
	Queue, // Duel: print the queue
	MatchInfo,
	Back, // to the main page
	Close,
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

// What the pages show: the adapter fills it from the level each refresh.
struct MenuView
{
	std::string hostname;
	std::string gametype; // its title: "Free For All"
	std::string map;      // the level's title, else its file name
	std::string map_file; // "q2dm1"
	std::string ruleset;

	bool teams = false; // TDM, CTF
	bool ctf = false;
	bool duel = false;

	bool playing = false;  // in the match (on a team, in FFA, duelling)
	bool chasing = false;  // a spectator following someone
	int team = 0;          // teams: 0 none, 1 red, 2 blue
	int red = 0, blue = 0; // teams: players on each
	int players = 0;       // FFA, Duel: playing clients

	bool ready_up = false; // warmup or countdown with ready-up: show Ready / Not Ready
	bool ready = false;
	bool ready_required = false; // match_ready

	int timelimit_min = 0;
	int fraglimit = 0;
	int capturelimit = 0;
	int overtime = 0; // match_overtime: minutes, -1 sudden death, 0 none
	bool rounds = false;
	bool match_flow = false; // match_warmup: warmup, countdown and the HUD
};

// The main page, and the row its cursor starts on (team modes: the team
// with fewer players, else the first choice).
MenuPage Core_Menu_Main(const MenuView &view);
int Core_Menu_MainCursor(const MenuPage &page, const MenuView &view);

// The settings the match runs under.
MenuPage Core_Menu_MatchInfo(const MenuView &view);
