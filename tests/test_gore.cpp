#include "core/gore.h"

#include "doctest.h"

TEST_CASE("no blood: off by default changes nothing")
{
	CHECK_FALSE(Core_Gore_HitToSparks(false, true));
	CHECK(Core_Gore_GibLook(false, false, false) == GibLook::Stock);
	CHECK(Core_Gore_GibLook(false, true, false) == GibLook::Stock);
}

TEST_CASE("no blood: blood hits become sparks, other hits stay")
{
	CHECK(Core_Gore_HitToSparks(true, true));
	CHECK_FALSE(Core_Gore_HitToSparks(true, false));
}

TEST_CASE("no blood: flesh gibs hidden, metal kept without a trail, debris as stock")
{
	CHECK(Core_Gore_GibLook(true, false, false) == GibLook::Hidden);
	CHECK(Core_Gore_GibLook(true, true, false) == GibLook::NoTrail);
	CHECK(Core_Gore_GibLook(true, false, true) == GibLook::Stock);
	CHECK(Core_Gore_GibLook(true, true, true) == GibLook::Stock);
}
