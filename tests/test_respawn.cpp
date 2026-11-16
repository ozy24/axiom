#include "doctest.h"

#include "core/respawn.h"

#include <cmath>

TEST_CASE("respawn: fire respawns after a second, whatever the forced time")
{
	const RespawnTimes t = Core_Respawn_Times(10000, true, 3.0f);
	CHECK(t.may_ms == 11000);
	CHECK(t.forced_ms == 13000);
	CHECK(Core_Respawn_Times(10000, false, 3.0f).may_ms == 11000); // stock would wait 3 s here
}

TEST_CASE("respawn: no forced respawn unless g_dm_force_respawn is on")
{
	CHECK(Core_Respawn_Times(10000, false, 3.0f).forced_ms == 0);
	CHECK(Core_Respawn_Times(10000, false, 0.0f).forced_ms == 0);
}

TEST_CASE("respawn: a forced time under the second is the second")
{
	CHECK(Core_Respawn_Times(10000, true, 0.0f).forced_ms == 11000);
	CHECK(Core_Respawn_Times(10000, true, 0.5f).forced_ms == 11000);
	CHECK(Core_Respawn_Times(10000, true, -5.0f).forced_ms == 11000);
	CHECK(Core_Respawn_Times(10000, true, std::nanf("")).forced_ms == 11000);
	CHECK(Core_Respawn_Times(10000, true, 1.5f).forced_ms == 11500);
	CHECK(Core_Respawn_Times(0, true, 1e30f).forced_ms == 3600000); // capped at an hour
}
