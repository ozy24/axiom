#include "core/scoreboard.h"

#include "doctest.h"

#include <algorithm>
#include <sstream>
#include <string>

namespace
{
ScoreboardPlayer Player(int slot, int frags, int deaths = 0, bool spectator = false)
{
	ScoreboardPlayer p;
	p.slot = slot;
	p.frags = frags;
	p.deaths = deaths;
	p.ping = 20;
	p.played_ms = 6 * 60000 + 38000;
	p.spectator = spectator;
	return p;
}

// The `ctf` rows' client numbers, in order.
std::vector<int> Slots(const std::string &layout)
{
	std::vector<int> out;
	for (size_t at = layout.find("ctf "); at != std::string::npos; at = layout.find("ctf ", at + 1))
	{
		std::istringstream row(layout.substr(at + 4));
		int x = 0, y = 0, slot = 0;
		if (row >> x >> y >> slot)
			out.push_back(slot);
	}
	return out;
}

size_t Count(const std::string &s, const std::string &what)
{
	size_t n = 0;
	for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + 1))
		++n;
	return n;
}
} // namespace

TEST_CASE("scoreboard: frags per hour over the time played")
{
	CHECK(Core_Scoreboard_Fph(42, 6 * 60000 + 38000) == 379); // the OpenFFA board's Xin
	CHECK(Core_Scoreboard_Fph(10, 3600000) == 10);
	CHECK(Core_Scoreboard_Fph(5, 0) == 0);
	CHECK(Core_Scoreboard_Fph(-2, 1800000) == -4);
}

TEST_CASE("scoreboard: by frags, then fewer deaths, then slot")
{
	const auto order = Core_Scoreboard_Order({Player(0, 5, 3), Player(1, 9), Player(2, 5, 1), Player(3, 5, 1)});
	std::vector<int> slots;
	for (const ScoreboardPlayer &p : order)
		slots.push_back(p.slot);
	CHECK(slots == std::vector<int>{1, 2, 3, 0});
}

TEST_CASE("scoreboard: a row is frags, deaths and the name, then FPH, whole minutes and ping")
{
	ScoreboardInput in;
	in.players = {Player(3, 42, 15)};
	in.stamp = "2026-10-29 19:40";
	const std::string layout = Core_Scoreboard_Layout(in);
	CHECK(layout.find("cstring2 \"[2026-10-29 19:40]\"") != std::string::npos);
	CHECK(layout.find("string2 Frg") != std::string::npos);
	CHECK(layout.find("ctf -40 36 3 42 15 \"\" xv -64 string 1 xv 166 string 379 xv 206 string 6 xv 246 string 20 ") !=
	      std::string::npos);
}

TEST_CASE("scoreboard: your own row's columns are green; spectators follow")
{
	ScoreboardInput in;
	in.players = {Player(0, 3), Player(1, 7), Player(2, 0, 0, true)};
	in.viewer = 0;
	const std::string layout = Core_Scoreboard_Layout(in);
	CHECK(Slots(layout) == std::vector<int>{1, 0, 2});
	CHECK(Count(layout, " string2 ") == 6 + 4); // the titles, and your position and three numbers
	CHECK(layout.find("ctf -40 44 0 3 0 \"\" xv -64 string2 2 ") != std::string::npos); // you, second
	CHECK(layout.find("$g_pc_spectators") != std::string::npos);
}

TEST_CASE("scoreboard: a full server fits the budget, keeping your own row")
{
	ScoreboardInput in;
	for (int i = 0; i < 32; ++i)
		in.players.push_back(Player(i, 100 - i, 999, i >= 24));
	in.stamp = "2026-10-29 19:40";
	in.viewer = 20; // 21st of 24 players
	const std::string layout = Core_Scoreboard_Layout(in);
	CHECK(layout.size() <= in.budget);
	const std::vector<int> slots = Slots(layout);
	REQUIRE(slots.size() >= 9); // 8 players (the worst case: 3-digit frags and FPH) and a spectator
	CHECK(slots[0] == 0);
	CHECK(std::find(slots.begin(), slots.end(), 20) != slots.end());
	CHECK(layout.find(" 20 80 999 \"\" xv -64 string2 21 ") !=
	      std::string::npos); // your own row last, at your real position
	MESSAGE("rows that fit: " << slots.size() << " in " << layout.size() << " bytes");

	in.viewer = 30; // a spectator: shown first of them
	const std::vector<int> watching = Slots(Core_Scoreboard_Layout(in));
	CHECK(std::find(watching.begin(), watching.end(), 30) != watching.end());
}

TEST_CASE("scoreboard: big numbers stay within the client's caps")
{
	ScoreboardInput in;
	ScoreboardPlayer p = Player(0, 5000, 5000);
	p.ping = 5000;
	p.played_ms = 1000;
	in.players = {p};
	const std::string layout = Core_Scoreboard_Layout(in);
	CHECK(layout.find("ctf -40 36 0 5000 999 ") != std::string::npos);
	CHECK(layout.find("string 9999 ") != std::string::npos);
	CHECK(layout.find("string 999 ") != std::string::npos);
}
