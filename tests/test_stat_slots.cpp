#include "doctest.h"

#include "core/stat_slots.h"

TEST_CASE("slots are handed out in order from the free range")
{
	StatSlots slots(54, 64);
	CHECK(slots.Remaining() == 10);
	CHECK(slots.Allocate("duel.opponent") == 54);
	CHECK(slots.Allocate("duel.score", 2) == 55);
	CHECK(slots.Remaining() == 7);
	CHECK(slots.Describe() == std::vector<std::string>{"duel.opponent: 54..54", "duel.score: 55..56"});
}

TEST_CASE("asking again with the same owner returns the same slots")
{
	StatSlots slots(54, 64);
	REQUIRE(slots.Allocate("hud.timer", 2) == 54);
	CHECK(slots.Allocate("hud.timer", 2) == 54);
	CHECK(slots.Remaining() == 8);
	CHECK_FALSE(slots.Allocate("hud.timer", 3).has_value()); // same owner, different size
}

TEST_CASE("the pool never overflows")
{
	StatSlots slots(54, 64);
	CHECK(slots.Allocate("big", 10) == 54);
	CHECK(slots.Remaining() == 0);
	CHECK_FALSE(slots.Allocate("one more").has_value());
	StatSlots small(60, 64);
	CHECK_FALSE(small.Allocate("too big", 5).has_value());
	CHECK(small.Allocate("fits", 4) == 60);
}

TEST_CASE("bad requests are refused")
{
	StatSlots slots(54, 64);
	CHECK_FALSE(slots.Allocate("", 1).has_value());
	CHECK_FALSE(slots.Allocate("zero", 0).has_value());
	CHECK_FALSE(slots.Allocate("negative", -1).has_value());
	CHECK(slots.Remaining() == 10);
}
