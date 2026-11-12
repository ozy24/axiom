#include "core/entities.h"

#include "doctest.h"

namespace
{
EntitySpawnMode Mode(const char *gametype, bool teams = false, bool powerups = true)
{
	return {gametype, teams, powerups};
}
} // namespace

TEST_CASE("entities: mm-fleshref's Power Screen in FFA, Combat Armour elsewhere")
{
	const EntityKeys screen = {{"gametype", "ffa"}};
	const EntityKeys armour = {{"not_gametype", "ffa"}};
	CHECK_FALSE(Core_Entities_Excluded(screen, Mode("ffa")));
	CHECK(Core_Entities_Excluded(armour, Mode("ffa")));
	for (const char *other : {"duel", "tdm", "ctf"})
	{
		CHECK(Core_Entities_Excluded(screen, Mode(other)));
		CHECK_FALSE(Core_Entities_Excluded(armour, Mode(other)));
	}
}

TEST_CASE("entities: gametype lists, MuffMode's names, any case")
{
	CHECK_FALSE(Core_Entities_Excluded({{"gametype", "ffa tdm"}}, Mode("tdm")));
	CHECK_FALSE(Core_Entities_Excluded({{"gametype", "ffa,tdm"}}, Mode("tdm")));
	CHECK(Core_Entities_Excluded({{"gametype", "ffa, tdm"}}, Mode("duel")));
	CHECK_FALSE(Core_Entities_Excluded({{"gametype", "tournament"}}, Mode("duel")));
	CHECK_FALSE(Core_Entities_Excluded({{"gametype", "team"}}, Mode("tdm")));
	CHECK(Core_Entities_Excluded({{"not_gametype", "tournament"}}, Mode("duel")));
	CHECK_FALSE(Core_Entities_Excluded({{"GameType", "FFA"}}, Mode("ffa")));
	CHECK(Core_Entities_Excluded({{"gametype", "ca"}}, Mode("tdm"))); // a mode the mod doesn't have
}

TEST_CASE("entities: notteam, notfree")
{
	CHECK(Core_Entities_Excluded({{"notteam", "1"}}, Mode("tdm", true)));
	CHECK_FALSE(Core_Entities_Excluded({{"notteam", "1"}}, Mode("ffa")));
	CHECK(Core_Entities_Excluded({{"notfree", "1"}}, Mode("duel")));
	CHECK_FALSE(Core_Entities_Excluded({{"notfree", "1"}}, Mode("ctf", true)));
	CHECK_FALSE(Core_Entities_Excluded({{"notteam", "0"}}, Mode("tdm", true)));
}

TEST_CASE("entities: powerups_on and powerups_off follow the powerups setting")
{
	// mm-kmach: Adrenaline with powerups, a Large Health without
	const EntityKeys adrenaline = {{"powerups_on", "1"}};
	const EntityKeys health = {{"powerups_off", "1"}};
	CHECK_FALSE(Core_Entities_Excluded(adrenaline, Mode("ffa", false, true)));
	CHECK(Core_Entities_Excluded(health, Mode("ffa", false, true)));
	CHECK(Core_Entities_Excluded(adrenaline, Mode("ffa", false, false)));
	CHECK_FALSE(Core_Entities_Excluded(health, Mode("ffa", false, false)));
	// mm-conven: a Combat Armour under its Quad, meant for no powerups
	CHECK(Core_Entities_Excluded({{"no_powerups", "1"}}, Mode("ffa", false, true)));
	CHECK_FALSE(Core_Entities_Excluded({{"no_powerups", "1"}}, Mode("ffa", false, false)));
}

TEST_CASE("entities: MuffMode's rulesets, playing as Quake II")
{
	CHECK(Core_Entities_Excluded({{"notq2", "1"}}, Mode("ffa")));
	CHECK_FALSE(Core_Entities_Excluded({{"notq3a", "1"}}, Mode("ffa")));
	CHECK_FALSE(Core_Entities_Excluded({{"notq3", "1"}, {"notq1", "1"}}, Mode("ffa")));
	CHECK(Core_Entities_Excluded({{"ruleset", "q3 q4"}}, Mode("ffa")));
	CHECK_FALSE(Core_Entities_Excluded({{"ruleset", "q2"}}, Mode("ffa")));
	CHECK_FALSE(Core_Entities_Excluded({{"ruleset", "q3a mm"}}, Mode("ffa")));
	CHECK(Core_Entities_Excluded({{"not_ruleset", "q2re"}}, Mode("ffa")));
	CHECK_FALSE(Core_Entities_Excluded({{"not_ruleset", "q3 q4"}}, Mode("ffa")));
}

TEST_CASE("entities: the BFG always allowed")
{
	CHECK_FALSE(Core_Entities_Excluded({{"bfg_on", "1"}}, Mode("ffa")));
	CHECK(Core_Entities_Excluded({{"bfg_off", "1"}}, Mode("ffa")));
}

TEST_CASE("entities: other known keys change nothing, and are known")
{
	CHECK_FALSE(Core_Entities_Excluded({{"notarena", "1"}, {"author", "x"}, {"nobots", "1"}}, Mode("tdm", true)));
	CHECK_FALSE(Core_Entities_Excluded({}, Mode("ffa")));
	for (const char *key : {"gametype", "NOT_GAMETYPE", "notarena", "no_dm_spawnpads", "remasterer", "notq3a"})
		CHECK(Core_Entities_IsKey(key));
	CHECK_FALSE(Core_Entities_IsKey("classname"));
	CHECK_FALSE(Core_Entities_IsKey("gametypes"));
}

TEST_CASE("entities: MuffMode's small and large ammo are stock ammo")
{
	const AmmoAlias *slugs = Core_Entities_AmmoAlias("ammo_slugs_small");
	REQUIRE(slugs);
	CHECK(std::string(slugs->stock) == "ammo_slugs");
	CHECK(slugs->count == 3);
	CHECK(std::string(slugs->model) == "models/vault/items/ammo/slugs/small/tris.md2");
	REQUIRE(Core_Entities_AmmoAlias("ammo_bullets_large"));
	CHECK(Core_Entities_AmmoAlias("ammo_bullets_large")->count == 100);
	CHECK_FALSE(Core_Entities_AmmoAlias("ammo_bullets"));
	CHECK_FALSE(Core_Entities_AmmoAlias("ammo_rockets_large")); // MuffMode has none
}

TEST_CASE("entities: a jump pad peaks at its target")
{
	EntityVec v{};
	REQUIRE(Core_Entities_JumpVelocity({0, 0, 0}, {300, -400, 200}, 800, v));
	// up: 200 = vz^2 / 2g; across: 500 units in the time to the top
	const float time = v[2] / 800;
	CHECK(v[2] * v[2] / (2 * 800) == doctest::Approx(200));
	CHECK(v[0] * time == doctest::Approx(300));
	CHECK(v[1] * time == doctest::Approx(-400));
	REQUIRE(Core_Entities_JumpVelocity({0, 0, 0}, {0, 0, 100}, 800, v)); // straight up
	CHECK(v[0] == 0);
	CHECK_FALSE(Core_Entities_JumpVelocity({0, 0, 100}, {50, 0, 100}, 800, v)); // no height
	CHECK_FALSE(Core_Entities_JumpVelocity({0, 0, 0}, {50, 0, 100}, 0, v));
}

TEST_CASE("entities: a .ent file must be well-formed and begin with worldspawn")
{
	std::string error;
	CHECK(Core_Entities_ValidFile(
	    "{\n\"classname\" \"worldspawn\"\n}\n{ \"classname\" \"item_quad\" \"origin\" \"0 0 0\" }", error));
	CHECK(Core_Entities_ValidFile("// a comment\n{ \"classname\" \"worldspawn\" \"message\" \"a { b }\" }", error));
	CHECK_FALSE(Core_Entities_ValidFile("", error));
	CHECK(error == "no entities");
	CHECK_FALSE(Core_Entities_ValidFile("{ \"classname\" \"item_quad\" }", error));
	CHECK(error == "the first entity isn't worldspawn");
	CHECK_FALSE(Core_Entities_ValidFile("{ \"classname\" \"worldspawn\" }\n{ \"classname\" \"item_quad\"", error));
	CHECK(error == "entity 2 isn't closed");
	CHECK_FALSE(Core_Entities_ValidFile("{ \"classname\" \"worldspawn }", error));
	CHECK(error == "an unclosed quote");
	CHECK_FALSE(Core_Entities_ValidFile("{ \"classname\" \"worldspawn\" } junk", error));
	CHECK(error == "expected { at entity 2");
	CHECK_FALSE(Core_Entities_ValidFile("{ \"classname\" }", error)); // a key without its value
}

TEST_CASE("entities: g_disable_items, any case, spaces or commas")
{
	CHECK(Core_Entities_Disabled("item_power_shield", "item_power_shield"));
	CHECK(Core_Entities_Disabled("ITEM_POWER_SHIELD", "item_power_shield"));
	CHECK(Core_Entities_Disabled("item_quad, item_power_shield", "item_power_shield"));
	CHECK(Core_Entities_Disabled("item_quad item_power_screen weapon_bfg", "weapon_bfg"));
	CHECK(Core_Entities_Disabled("ammo_slugs", "ammo_slugs"));
	CHECK_FALSE(Core_Entities_Disabled("", "item_power_shield"));
	CHECK_FALSE(Core_Entities_Disabled("item_power_shield", "item_power_screen"));
	CHECK_FALSE(Core_Entities_Disabled("item_power", "item_power_shield")); // whole words only
}

TEST_CASE("entities: g_disable_items leaves everything but pickups alone")
{
	CHECK_FALSE(Core_Entities_Disabled("info_player_deathmatch", "info_player_deathmatch"));
	CHECK_FALSE(Core_Entities_Disabled("trigger_push misc_teleporter", "trigger_push"));
	CHECK_FALSE(Core_Entities_Disabled("item_flag_team1", "item_flag_team1"));
	CHECK_FALSE(Core_Entities_Disabled("item_", "item_"));
}
