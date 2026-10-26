#include "doctest.h"

#include "gametypes/duel/duel_queue.h"

#include <initializer_list>

namespace
{
// Keys as in the adapter would be social IDs; plain numbers keep tests short.
std::vector<std::string> K(std::initializer_list<int> ids)
{
	std::vector<std::string> out;
	for (int id : ids)
		out.push_back(std::to_string(id));
	return out;
}
} // namespace

TEST_CASE("the first two to arrive play, the rest wait in order")
{
	DuelQueue q;
	q.Sync(K({3, 1}));
	CHECK(q.Players() == K({3, 1}));
	q.Sync(K({3, 1, 7, 5}));
	CHECK(q.Players() == K({3, 1}));
	CHECK(q.Order() == K({3, 1, 7, 5}));
	CHECK(q.IsPlayer("1"));
	CHECK_FALSE(q.IsPlayer("7"));
}

TEST_CASE("Sync keeps places regardless of the order clients are listed in")
{
	DuelQueue q;
	q.Sync(K({2, 4, 6}));
	q.Sync(K({6, 4, 2, 8}));
	CHECK(q.Order() == K({2, 4, 6, 8}));
}

TEST_CASE("a duellist who leaves is replaced by the next in line")
{
	DuelQueue q;
	q.Sync(K({1, 2, 3}));
	q.Sync(K({1, 3}));
	CHECK(q.Players() == K({1, 3}));
}

TEST_CASE("the loser goes to the back; the winner stays on")
{
	DuelQueue q;
	q.Sync(K({1, 2, 3, 4}));
	q.Rotate("2");
	CHECK(q.Players() == K({1, 3}));
	CHECK(q.Order() == K({1, 3, 4, 2}));
	q.Rotate("1"); // the old winner loses next time
	CHECK(q.Players() == K({3, 4}));
}

TEST_CASE("one or no clients: fewer than two players, nothing breaks")
{
	DuelQueue q;
	CHECK(q.Players().empty());
	q.Sync(K({9}));
	CHECK(q.Players() == K({9}));
	q.Rotate("42"); // unknown: ignored
	CHECK(q.Order() == K({9}));
}

TEST_CASE("a level change keeps the rotation while clients re-enter one by one")
{
	DuelQueue q;
	q.Sync(K({0, 1, 2})); // bots 0 and 1 duel, the human (2) waits
	q.Rotate("1");        // 1 lost: the order is now 0, 2, 1
	REQUIRE(q.Order() == K({0, 2, 1}));
	// The next level: the bots begin first, the human a few seconds later.
	q.Sync(K({0}), true);
	q.Sync(K({0, 1}), true);
	CHECK(q.Order() == K({0, 2, 1}));
	CHECK_FALSE(q.IsPlayer("1")); // the loser doesn't take the absent human's place
	q.Sync(K({0, 1, 2}), true);
	CHECK(q.Players() == K({0, 2}));
	// After the grace period an absentee who never came back is dropped.
	q.Sync(K({0, 1}));
	CHECK(q.Players() == K({0, 1}));
}

TEST_CASE("a human keeps their place by key, whichever slot they come back in")
{
	DuelQueue q;
	q.Sync({"bot:0", "bot:1", "human-abc"});
	q.Rotate("bot:1");
	// Next level: the human reconnects and, keyed by social ID, the slot doesn't matter.
	q.Sync({"bot:0", "bot:1"}, true);
	q.Sync({"bot:0", "bot:1", "human-abc"}, true);
	CHECK(q.Players() == std::vector<std::string>{"bot:0", "human-abc"});
}
