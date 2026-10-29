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
// then the spectators that still fit. In ready-up, a tick right of the Ping
// column marks each ready player, while the bytes allow (rows come first).
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
	std::string stamp;     // the header: "2026-10-29 19:40"
	bool ready_up = false; // show the ready ticks
	size_t budget = 980;   // bytes it may use: stock's 1024, less its clock and frag limit
};

// Frags per hour: frags over the time played; 0 before a second has passed.
int Core_Scoreboard_Fph(int frags, int64_t played_ms);

// The players in board order: by frags, then fewer deaths, then slot.
std::vector<ScoreboardPlayer> Core_Scoreboard_Order(std::vector<ScoreboardPlayer> players);

// The layout text, at most `budget` bytes.
std::string Core_Scoreboard_Layout(const ScoreboardInput &in);

// The ready tick (MuffMode's, and Duel's board's): the weapon wheel's
// selected-compass picture, its top 2 above row `y`.
std::string Core_Scoreboard_Tick(int x, int y);

// The team board's ticks, for stock's CTFScoreboardMessage: one left of each
// team row (`ctf -40 ...` RED, `ctf 200 ...` BLUE) in `layout` whose client
// is in `ready`, beside the captain's `*`, while the layout stays under
// `max` bytes. Returns the text to append.
std::string Core_Scoreboard_TeamTicks(std::string_view layout, const std::set<int> &ready, size_t max);
