#include "core/miniscore.h"

#include "doctest.h"

namespace
{
MatchPlayer P(int id, int score, bool playing = true)
{
	MatchPlayer p;
	p.id = id;
	p.score = score;
	p.playing = playing;
	return p;
}

using Rows = std::array<int, 2>;
} // namespace

TEST_CASE("miniscore: the top two, best first")
{
	const std::vector<MatchPlayer> players{P(0, 3), P(1, 10), P(2, 7), P(3, 5)};
	CHECK(Core_Miniscore_Rows(players) == Rows{1, 2});
}

TEST_CASE("miniscore: ties go to the lower slot")
{
	CHECK(Core_Miniscore_Rows({P(2, 8), P(0, 8), P(1, 8)}) == Rows{0, 1});
	CHECK(Core_Miniscore_Rows({P(3, 1), P(1, 6), P(2, 6)}) == Rows{1, 2});
}

TEST_CASE("miniscore: only playing players count")
{
	const std::vector<MatchPlayer> players{P(0, 2), P(1, 6), P(2, 9, false), P(3, 4)};
	CHECK(Core_Miniscore_Rows(players) == Rows{1, 3});
}

TEST_CASE("miniscore: empty rows with fewer than two players")
{
	CHECK(Core_Miniscore_Rows({}) == Rows{-1, -1});
	CHECK(Core_Miniscore_Rows({P(4, 0)}) == Rows{4, -1});
	CHECK(Core_Miniscore_Rows({P(4, 0, false), P(5, 1, false)}) == Rows{-1, -1});
}

TEST_CASE("miniscore: negative scores still rank")
{
	CHECK(Core_Miniscore_Rows({P(0, -5), P(1, -2), P(2, -9)}) == Rows{1, 0});
}

TEST_CASE("miniscore: the face is the skin's icon")
{
	CHECK(Core_Miniscore_IconPath("male/grunt") == "/players/male/grunt_i");
	CHECK(Core_Miniscore_IconPath("Female/Athena") == "/players/female/athena_i");
	CHECK(Core_Miniscore_IconPath("cyborg/ctf_r") == "/players/cyborg/ctf_r_i");
	CHECK(Core_Miniscore_IconPath("my-model/skin_2") == "/players/my-model/skin_2_i");
}

TEST_CASE("miniscore: odd skin names get no face")
{
	for (const char *skin : {"", "male", "/grunt", "male/", "male/../../pics/x", "a/b/c", "male/gr unt", "male\\grunt",
	                         "male/grunt;quit", "../grunt"})
		CHECK_MESSAGE(Core_Miniscore_IconPath(skin).empty(), skin);
	CHECK(Core_Miniscore_IconPath(std::string(70, 'a') + "/b").empty());
}

TEST_CASE("miniscore: distinct faces per level are capped")
{
	MiniscoreIcons icons(2);
	CHECK(icons.Admit("/players/male/grunt_i"));
	CHECK(icons.Admit("/players/female/athena_i"));
	CHECK_FALSE(icons.Admit("/players/cyborg/oni911_i"));
	CHECK(icons.Admit("/players/male/grunt_i")); // already in: no new image
	icons.Reset();
	CHECK(icons.Admit("/players/cyborg/oni911_i"));
}

TEST_CASE("match: in play from live to the end of the rounds, not around them")
{
	CHECK(MatchStateInPlay(MatchState::Live));
	CHECK(MatchStateInPlay(MatchState::Overtime));
	CHECK(MatchStateInPlay(MatchState::RoundOver));
	CHECK(MatchStateInPlay(MatchState::RoundCountdown));
	CHECK_FALSE(MatchStateInPlay(MatchState::Warmup));
	CHECK_FALSE(MatchStateInPlay(MatchState::Countdown));
	CHECK_FALSE(MatchStateInPlay(MatchState::Intermission));
}
