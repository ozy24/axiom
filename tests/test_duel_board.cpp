#include "gametypes/duel/duel_board.h"

#include "doctest.h"

#include <string>

namespace
{
bool Has(const std::string &layout, const std::string &part)
{
	return layout.find(part) != std::string::npos;
}

DuelBoardInput TwoPlaying()
{
	DuelBoardInput in;
	in.title = "Duel on The Edge";
	DuelBoardDuellist a, b;
	a.slot = 0;
	a.score = 10;
	a.ping = 17;
	a.minutes = 6;
	b.slot = 1;
	b.score = 7;
	b.ping = 15;
	b.minutes = 6;
	in.duellists = {a, b};
	return in;
}
} // namespace

TEST_CASE("duel board: the title and stock's two blocks")
{
	const std::string layout = Duel_BoardLayout(TwoPlaying());
	CHECK(Has(layout, "xv 0 yv -40 cstring2 \"Duel on The Edge\" "));
	CHECK(Has(layout, "xv -40 yv 0 dogtag 0 client -72 0 0 10 17 6 "));
	CHECK(Has(layout, "xv 162 yv 0 dogtag 1 client 130 0 1 7 15 6 "));
	CHECK_FALSE(Has(layout, "WINNER"));
	CHECK_FALSE(Has(layout, "Next up"));
}

TEST_CASE("duel board: ready ticks only in ready-up; no records (Chris, 2026-10-30)")
{
	DuelBoardInput in = TwoPlaying();
	in.duellists[0].ready = true;
	in.viewer = 1;
	CHECK_FALSE(Has(Duel_BoardLayout(in), "p_compass_selected"));
	in.ready_up = true;
	const std::string layout = Duel_BoardLayout(in);
	CHECK(Has(layout, "xv -56 yv 16 picn wheel/p_compass_selected "));
	CHECK_FALSE(Has(layout, "xv 146 yv 16 picn"));
	CHECK_FALSE(Has(layout, "yv 36"));
}

TEST_CASE("duel board: the queue's place and ping, then the spectators' 0 and ping")
{
	DuelBoardInput in = TwoPlaying();
	in.queue = {{4, 30}, {2, 1500}};
	in.spectators = {{3, 20}, {5, 45}, {6, 0}};
	in.viewer = 2;
	const std::string layout = Duel_BoardLayout(in);
	CHECK(Has(layout, "xv -13 yv 44 picn ping xv 14 string2 Queue "));
	CHECK(Has(layout, "ctf -40 52 4 1 30 \"\" "));
	CHECK(Has(layout, "ctf -40 60 2 2 999 \"\" ")); // you, second in line: the row is green itself
	CHECK_FALSE(Has(layout, " W "));
	CHECK(Has(layout, "xv -13 yv 76 picn ping xv 14 loc_string2 0 \"$g_pc_spectators\""));
	CHECK(Has(layout, "ctf -40 84 3 0 20 \"\" ctf 160 84 5 0 45 \"\" ctf -40 92 6 0 0 \"\" "));
}

TEST_CASE("duel board: the winner and the match time at intermission")
{
	DuelBoardInput in = TwoPlaying();
	in.winner = 0;
	in.match_time = "10:00";
	const std::string layout = Duel_BoardLayout(in);
	CHECK(Has(layout, "xv 0 yv -30 cstring2 \"Match time 10:00\" "));
	CHECK(Has(layout, "xv -32 yv -20 string2 WINNER "));
}

TEST_CASE("duel board: a crowd stays within the budget")
{
	DuelBoardInput in = TwoPlaying();
	for (int i = 2; i < 32; ++i)
		(i < 16 ? in.queue : in.spectators).push_back({i, 999});
	in.budget = 700;
	const std::string layout = Duel_BoardLayout(in);
	CHECK(layout.size() <= 700);
	CHECK(Has(layout, "client 130 0 1 "));
}
