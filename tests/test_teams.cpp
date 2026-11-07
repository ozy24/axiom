#include "doctest.h"

#include "core/teams.h"

#include <algorithm>
#include <cstdlib>
#include <set>

namespace
{
using Kind = TeamsEvent::Kind;

TeamsPlayer Red(std::string key)
{
	return {std::move(key), 1, false};
}
TeamsPlayer Blue(std::string key)
{
	return {std::move(key), 2, false};
}
TeamsPlayer Bot(std::string key, int team)
{
	return {std::move(key), team, true};
}
} // namespace

TEST_CASE("the first human on a team is its captain; a bot never is")
{
	Teams t;
	auto events = t.Sync({Bot("b1", 1), Red("a")});
	CHECK(t.Captain(1) == "a");
	CHECK(events == std::vector<TeamsEvent>{{Kind::Captain, 1, "a"}});
	CHECK(t.Sync({Bot("b1", 1), Red("a"), Red("b")}).empty());
	CHECK(t.Captain(1) == "a");

	Teams bots;
	bots.Sync({Bot("b1", 2), Bot("b2", 2)});
	CHECK(bots.Captain(2).empty());
}

TEST_CASE("a captain who leaves is replaced by the human longest on the team")
{
	Teams t;
	t.Sync({Red("a")});
	t.Sync({Red("a"), Red("b")});
	t.Sync({Red("a"), Red("b"), Red("c")});
	// b switches away and back: their time on red starts again, after c's.
	t.Sync({Red("a"), Blue("b"), Red("c")});
	t.Sync({Red("a"), Red("b"), Red("c")});
	auto events = t.Sync({Red("b"), Red("c")}); // a disconnects
	CHECK(t.Captain(1) == "c");
	CHECK(events == std::vector<TeamsEvent>{{Kind::Captain, 1, "c"}});
	// Moving to the other team is leaving too.
	t.Sync({Red("b"), Blue("c")});
	CHECK(t.Captain(1) == "b");
	CHECK(t.Captain(2) == "c");
	// The last human gone: no captain.
	t.Sync({Bot("x", 1), Blue("c")});
	CHECK(t.Captain(1).empty());
}

TEST_CASE("a locked team takes only its roster; bots and leaving are never refused")
{
	Teams t;
	t.Sync({Red("a"), Red("b"), Blue("c")});
	CHECK(t.Lock(1) == TeamsResult::Ok);
	CHECK(t.Lock(1) == TeamsResult::Already);
	CHECK(t.Locked(1));
	CHECK_FALSE(t.Locked(2));
	CHECK(t.MayJoin("a", 1, false));
	CHECK_FALSE(t.MayJoin("c", 1, false));
	CHECK_FALSE(t.MayJoin("new", 1, false));
	CHECK(t.MayJoin("new", 2, false));
	CHECK(t.MayJoin("botty", 1, true));
	CHECK(t.MayJoin("new", 0, false));
}

TEST_CASE("a rostered player who drops can come back to a locked team")
{
	Teams t;
	t.Sync({Red("a"), Red("b"), Blue("c")});
	t.Lock(1);
	t.Sync({Red("a"), Blue("c")}); // b drops
	CHECK(t.MayJoin("b", 1, false));
	t.Sync({Red("a"), Red("b"), Blue("c")}); // and is back, in any slot
	CHECK(t.OnRoster(1, "b"));
	// Leaving for spectating keeps the place too.
	t.Sync({Red("a"), {"b", 0, false}, Blue("c")});
	CHECK(t.MayJoin("b", 1, false));
}

TEST_CASE("unlock clears the roster; a later lock takes whoever is on the team then")
{
	Teams t;
	t.Sync({Red("a"), Red("b")});
	t.Lock(1);
	t.Sync({Red("a")}); // b leaves
	CHECK(t.Unlock(1) == TeamsResult::Ok);
	CHECK(t.Unlock(1) == TeamsResult::Already);
	CHECK(t.Roster(1).empty());
	t.Sync({Red("a"), Red("sub")});
	t.Lock(1);
	CHECK(t.OnRoster(1, "sub"));
	CHECK_FALSE(t.MayJoin("b", 1, false));
}

TEST_CASE("whoever is on a locked team joins its roster, however they got there")
{
	Teams t;
	t.Sync({Red("a")});
	t.Lock(1);
	t.Sync({Red("a"), Red("moved")}); // an admin moved them, or force-join placed them
	CHECK(t.OnRoster(1, "moved"));
}

TEST_CASE("a locked team with no humans left unlocks itself")
{
	Teams t;
	t.Sync({Red("a"), Bot("x", 1), Blue("c")});
	t.Lock(1);
	t.Lock(2);
	auto events = t.Sync({Bot("x", 1), Blue("c")});
	CHECK_FALSE(t.Locked(1));
	CHECK(t.Locked(2));
	CHECK(events == std::vector<TeamsEvent>{{Kind::Unlocked, 1, ""}});
	CHECK(t.MayJoin("anyone", 1, false));
	CHECK(t.Lock(1) == TeamsResult::Empty);
}

TEST_CASE("going live with match_lock locks every team with a human")
{
	Teams t;
	t.Sync({Red("a"), Bot("x", 2)});
	CHECK(t.GoLive(false).empty());
	CHECK_FALSE(t.Locked(1));
	auto events = t.GoLive(true);
	CHECK(events == std::vector<TeamsEvent>{{Kind::Locked, 1, ""}});
	CHECK(t.Locked(1));
	CHECK_FALSE(t.Locked(2));
	// Already locked by the captain: not announced again.
	CHECK(t.GoLive(true).empty());
}

TEST_CASE("Clear unlocks for the next warmup and keeps captains; Reset forgets everything")
{
	Teams t;
	t.Sync({Red("a"), Blue("c")});
	t.GoLive(true);
	t.Clear();
	CHECK_FALSE(t.Locked(1));
	CHECK_FALSE(t.Locked(2));
	CHECK(t.Roster(1).empty());
	CHECK(t.Captain(1) == "a");
	t.Reset();
	CHECK(t.Captain(1).empty());
	CHECK(t.TeamOf("a") == 0);
}

TEST_CASE("only the captain locks, unlocks, hands over, kicks and readies the team")
{
	Teams t;
	t.Sync({Red("a"), Red("b"), Bot("x", 1), Blue("c")});
	std::vector<std::string> keys;
	CHECK(t.CaptainLock("b", true) == TeamsResult::NotCaptain);
	CHECK(t.CaptainLock("nobody", true) == TeamsResult::NoTeam);
	CHECK(t.Transfer("b", "a") == TeamsResult::NotCaptain);
	CHECK(t.Kick("b", "a") == TeamsResult::NotCaptain);
	CHECK(t.ReadyTeam("b", keys) == TeamsResult::NotCaptain);

	CHECK(t.CaptainLock("a", true) == TeamsResult::Ok);
	CHECK(t.Locked(1));
	CHECK_FALSE(t.Locked(2)); // their own team only
	CHECK(t.CaptainLock("a", false) == TeamsResult::Ok);
	CHECK(t.ReadyTeam("a", keys) == TeamsResult::Ok);
	CHECK(keys == std::vector<std::string>{"a", "b"});
}

TEST_CASE("a captain hands over only to a human teammate")
{
	Teams t;
	t.Sync({Red("a"), Red("b"), Bot("x", 1), Blue("c")});
	CHECK(t.Transfer("a", "a") == TeamsResult::Self);
	CHECK(t.Transfer("a", "c") == TeamsResult::NotTeammate);
	CHECK(t.Transfer("a", "x") == TeamsResult::Bot);
	CHECK(t.Transfer("a", "b") == TeamsResult::Ok);
	CHECK(t.Captain(1) == "b");
	// Kept: the old captain is still on the team, and that's no reason to take it back.
	CHECK(t.Sync({Red("a"), Red("b"), Bot("x", 1), Blue("c")}).empty());
	CHECK(t.Captain(1) == "b");
}

TEST_CASE("a kicked player is off the roster and can't come back while the team is locked")
{
	Teams t;
	t.Sync({Red("a"), Red("b"), Bot("x", 1)});
	t.Lock(1);
	CHECK(t.Kick("a", "a") == TeamsResult::Self);
	CHECK(t.Kick("a", "x") == TeamsResult::Bot);
	CHECK(t.Kick("a", "b") == TeamsResult::Ok);
	t.Sync({Red("a"), {"b", 0, false}, Bot("x", 1)}); // moved to spectating by the adapter
	CHECK_FALSE(t.MayJoin("b", 1, false));
	t.Unlock(1);
	CHECK(t.MayJoin("b", 1, false));
}

namespace
{
int Count(const std::vector<int> &teams, int team)
{
	return (int)std::count(teams.begin(), teams.end(), team);
}
} // namespace

TEST_CASE("shuffle: an even split of the same players")
{
	for (uint32_t seed = 0; seed < 50; ++seed)
	{
		std::mt19937 rng(seed);
		const std::vector<int> now{1, 1, 1, 1, 2, 2, 2};
		const std::vector<int> after = Core_Teams_Shuffle(now, rng);
		REQUIRE(after.size() == now.size());
		CHECK(Count(after, 1) + Count(after, 2) == 7);
		CHECK(std::abs(Count(after, 1) - Count(after, 2)) == 1);
	}
	std::mt19937 rng(1);
	const std::vector<int> lopsided = Core_Teams_Shuffle({1, 1, 1, 1, 1, 1}, rng);
	CHECK(Count(lopsided, 1) == 3);
	CHECK(Count(lopsided, 2) == 3);
}

TEST_CASE("shuffle: never the same two groups back, in either colour")
{
	for (uint32_t seed = 0; seed < 200; ++seed)
	{
		std::mt19937 rng(seed);
		const std::vector<int> three{1, 1, 2};
		const std::vector<int> a = Core_Teams_Shuffle(three, rng);
		CHECK(a != three);
		CHECK(a != std::vector<int>{2, 2, 1});
		const std::vector<int> four{1, 2, 1, 2};
		const std::vector<int> b = Core_Teams_Shuffle(four, rng);
		CHECK(b != four);
		CHECK(b != std::vector<int>{2, 1, 2, 1});
	}
}

TEST_CASE("shuffle: varies with the draw; two players or none are fine")
{
	std::set<std::vector<int>> seen;
	for (uint32_t seed = 0; seed < 100; ++seed)
	{
		std::mt19937 rng(seed);
		seen.insert(Core_Teams_Shuffle({1, 1, 1, 2, 2, 2}, rng));
	}
	CHECK(seen.size() > 10); // 20 splits of six into three and three, less the one now
	std::mt19937 rng(3);
	CHECK(Count(Core_Teams_Shuffle({1, 2}, rng), 1) == 1);
	CHECK(Core_Teams_Shuffle({}, rng).empty());
}

TEST_CASE("the server removes a player from either team's roster; never a bot")
{
	Teams t;
	t.Sync({Red("a"), Red("b"), Blue("c"), Blue("d"), Bot("x", 2)});
	t.Lock(1);
	t.Lock(2);
	CHECK(t.Remove("nobody") == TeamsResult::NoTeam);
	CHECK(t.Remove("x") == TeamsResult::Bot);
	CHECK(t.Remove("a") == TeamsResult::Ok); // the captain too: no captain needed
	CHECK(t.Remove("c") == TeamsResult::Ok);
	t.Sync({{"a", 0, false}, Red("b"), {"c", 0, false}, Blue("d"), Bot("x", 2)}); // moved to spectating
	CHECK_FALSE(t.MayJoin("a", 1, false));
	CHECK_FALSE(t.MayJoin("c", 2, false));
	CHECK(t.MayJoin("b", 1, false));
}
