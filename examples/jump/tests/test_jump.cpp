#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "gametypes/jump/jump.h"

TEST_CASE("jump: only the world hurts")
{
	CHECK_FALSE(Jump_Hurts(true, false)); // a rocket, your own included
	CHECK_FALSE(Jump_Hurts(false, true)); // a fall
	CHECK(Jump_Hurts(false, false));      // lava, trigger_hurt
}

TEST_CASE("jump: run times")
{
	CHECK(Jump_TimeText(12345) == "12.345 s");
	CHECK(Jump_TimeText(5) == "0.005 s");
	CHECK(Jump_TimeText(-1) == "0.000 s");
}
