// The column scoreboard outside team modes (g_scoreboard 1; docs/design/
// scoreboard.md, decision 0018), as pure logic: the layout text, fitted to
// the client's 1024 bytes. No engine access; the adapter is
// core/scoreboard_register.cpp.
//
//                    [2026-10-29 19:40]
//      Frg Dth Player           FPH Time Ping
//   1   42  15 Xin              379    6   17
//
// Only the layout's `ctf` row turns a KEX name token (`##P3`) into the name
// (a test on the KEX client, 2026-10-29), and it draws two numbers then the
// name: frags, deaths, name. The position (left of it) and the other columns
// are text of our own. Your own row is green (the `ctf` row's own
// highlight). Spectators follow, two to a line. When a full server doesn't
// fit, it shows the top players and your own row (at its real position),
// then the spectators that still fit. In ready-up, a green R right of the
// Ping column marks each ready player, while the bytes allow (rows come first).
#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct ScoreboardPlayer
{
	int slot = 0; // client number
	int frags = 0;
	int deaths = 0;
	int ping = 0;
	int64_t played_ms = 0; // time playing in this match
	bool spectator = false;
	bool ready = false; // ready-up: ready (bots always are)
};

struct ScoreboardInput
{
	std::vector<ScoreboardPlayer> players;
	int viewer = -1;       // the slot it's for
	bool ready_up = false; // show the ready marks
	size_t budget = 980;   // bytes it may use: stock's 1024, less its clock and frag limit
};

// Frags per hour: frags over the time played; 0 before a second has passed.
int Core_Scoreboard_Fph(int frags, int64_t played_ms);

// The players in board order: by frags, then fewer deaths, then slot.
std::vector<ScoreboardPlayer> Core_Scoreboard_Order(std::vector<ScoreboardPlayer> players);

// The layout text, at most `budget` bytes.
std::string Core_Scoreboard_Layout(const ScoreboardInput &in);

// The spectators' heading at `y` for a column of `ctf` rows at `x`: stock's
// ping icon (9x9, as the duel blocks show it) over the ping, "Spectators"
// over the names. A KEX name shows only in
// a `ctf` row, which always draws two numbers first (docs/design/scoreboard.md):
// a spectator's are 0 and their ping, never frags and deaths.
std::string Core_Scoreboard_SpectatorTitle(int x, int y);

// A spectator's `ctf` row: 0, ping (capped at 999), name.
std::string Core_Scoreboard_SpectatorRow(int x, int y, int slot, int ping);

// The ready mark on a row at `y`: a green R, one character, so it fits the
// row. (Duel's board ticks its blocks with a picture; a stock client draws
// that 24 pixels high, three rows, and covers the text: playtest 2026-10-29.)
std::string Core_Scoreboard_ReadyMark(int x, int y);

// The team board's ready marks, for stock's CTFScoreboardMessage: one left of
// each team row (`ctf -40 ...` RED, `ctf 200 ...` BLUE) in `layout` whose
// client is in `ready`, beside the captain's `*`, while the layout stays
// under `max` bytes. Returns the text to append.
std::string Core_Scoreboard_TeamReady(std::string_view layout, const std::set<int> &ready, size_t max);
