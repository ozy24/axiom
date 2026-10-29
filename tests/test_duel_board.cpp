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

TEST_CASE("duel records: wins, losses and the streak a loss ends")
{
	DuelRecords r;
	r.Result("xin", "aid");
	r.Result("xin", "oesd");
	CHECK(r.Get("xin").wins == 2);
	CHECK(r.Get("xin").streak == 2);
	CHECK(r.Get("aid").losses == 1);
	r.Result("aid", "xin");
	CHECK(r.Get("xin").streak == 0);
	CHECK(r.Get("xin").losses == 1);
	CHECK(r.Get("aid").streak == 1);
	CHECK(r.Get("nobody").wins == 0);
}

TEST_CASE("duel records: the text under a block")
{
	CHECK(Duel_RecordText({}).empty());
	CHECK(Duel_RecordText({1, 0, 1}) == "W 1  L 0");
	CHECK(Duel_RecordText({3, 1, 3}) == "W 3  L 1  streak 3");
}

TEST_CASE("duel board: the title and stock's two blocks")
{
	const std::string layout = Duel_BoardLayout(TwoPlaying());
	CHECK(Has(layout, "xv 0 yv -40 cstring2 \"Duel on The Edge\" "));
	CHECK(Has(layout, "xv -40 yv 0 dogtag 0 client -72 0 0 10 17 6 "));
	CHECK(Has(layout, "xv 162 yv 0 dogtag 1 client 130 0 1 7 15 6 "));
	CHECK_FALSE(Has(layout, "WINNER"));
	CHECK_FALSE(Has(layout, "Next up"));
}

TEST_CASE("duel board: ready ticks only in ready-up; records under the blocks")
{
	DuelBoardInput in = TwoPlaying();
	in.duellists[0].ready = true;
	in.duellists[1].record = {3, 1, 3};
	in.viewer = 1;
	CHECK_FALSE(Has(Duel_BoardLayout(in), "p_compass_selected"));
	in.ready_up = true;
	const std::string layout = Duel_BoardLayout(in);
	CHECK(Has(layout, "xv -56 yv 16 picn wheel/p_compass_selected "));
	CHECK_FALSE(Has(layout, "xv 146 yv 16 picn"));
	CHECK(Has(layout, "xv 170 yv 36 string2 \"W 3  L 1  streak 3\" ")); // your own record is green
}

TEST_CASE("duel board: the queue is numbered, then the spectators two to a line")
{
	DuelBoardInput in = TwoPlaying();
	in.queue = {{4, {0, 2, 0}}, {2, {}}};
	in.spectators = {{3, {}}, {5, {1, 4, 0}}, {6, {}}};
	in.viewer = 2;
	const std::string layout = Duel_BoardLayout(in);
	CHECK(Has(layout, "string2 \"Next up\" "));
	CHECK(Has(layout, "ctf -40 60 4 0 2 \"\" xv -64 string 1 "));
	CHECK(Has(layout, "ctf -40 68 2 0 0 \"\" xv -64 string2 2 ")); // you, second in line
	CHECK(Has(layout, "$g_pc_spectators"));
	CHECK(Has(layout, "ctf -40 92 3 0 0 \"\" ctf 160 92 5 1 4 \"\" ctf -40 100 6 0 0 \"\" "));
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
		(i < 16 ? in.queue : in.spectators).push_back({i, {999, 999, 0}});
	in.budget = 700;
	const std::string layout = Duel_BoardLayout(in);
	CHECK(layout.size() <= 700);
	CHECK(Has(layout, "client 130 0 1 "));
}
