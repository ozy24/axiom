#include "doctest.h"

#include "core/arena.h"

#include <algorithm>

TEST_CASE("arena loadout: every weapon but the BFG, the rocket launcher in hand, 150/150")
{
	const ArenaLoadout l = Core_Arena_Loadout(150, 150);
	CHECK(l.weapons.size() == 9);
	CHECK(std::find(l.weapons.begin(), l.weapons.end(), "weapon_railgun") != l.weapons.end());
	CHECK(std::find(l.weapons.begin(), l.weapons.end(), "weapon_bfg") == l.weapons.end());
	CHECK(l.in_hand == "weapon_rocketlauncher");
	CHECK(l.health == 150);
	CHECK(l.armor == 150);
}

TEST_CASE("arena loadout: health 1-999, armor 0-200 (body armor's most)")
{
	CHECK(Core_Arena_Loadout(0, -5).health == 1);
	CHECK(Core_Arena_Loadout(0, -5).armor == 0);
	CHECK(Core_Arena_Loadout(5000, 500).health == 999);
	CHECK(Core_Arena_Loadout(5000, 500).armor == 200);
}

TEST_CASE("damage score: a point per 100, the remainder kept")
{
	DamageScore d;
	CHECK(d.Add(1, 60, 150, 0) == 0);
	CHECK(d.Add(1, 30, 150, 20) == 1); // 60 + 50 = 110: one point, 10 left
	CHECK(d.Add(1, 90, 150, 0) == 1);  // 10 + 90
	CHECK(d.Add(2, 99, 150, 0) == 0);  // per attacker
	d.Forget(2);
	CHECK(d.Add(2, 1, 150, 0) == 0); // 99 forgotten
}

TEST_CASE("damage score: health taken counts only what the target had; armor absorbed counts")
{
	DamageScore d;
	CHECK(d.Add(1, 400, 30, 0) == 0);  // a 400 rocket on 30 health: 30
	CHECK(d.Add(1, 20, 150, 50) == 1); // 30 + 20 + 50
	CHECK(d.Add(1, 100, -10, 0) == 0); // a corpse has nothing to take
	d.Reset();
	CHECK(d.Add(1, 99, 150, 0) == 0);
}
