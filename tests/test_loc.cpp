#include "core/loc.h"

#include "doctest.h"

TEST_CASE("loc: a .loc line is x y z at eight times the map's, then the name")
{
	LocPlace p;
	REQUIRE(Core_Loc_ParseLine("800 -1600 64 Red Armor", p));
	CHECK(p.origin.x == 100.0f);
	CHECK(p.origin.y == -200.0f);
	CHECK(p.origin.z == 8.0f);
	CHECK(p.label == "Red Armor");
	REQUIRE(Core_Loc_ParseLine("  1.5e2\t0 0   upper  \t RL  ", p));
	CHECK(p.origin.x == doctest::Approx(18.75f));
	CHECK(p.label == "upper RL");
	CHECK_FALSE(Core_Loc_ParseLine("800 -1600 64", p));        // no name
	CHECK_FALSE(Core_Loc_ParseLine("800 -1600 Red Armor", p)); // two coordinates
	CHECK_FALSE(Core_Loc_ParseLine("8x0 0 0 RA", p));
	CHECK_FALSE(Core_Loc_ParseLine("nan 0 0 RA", p));
	CHECK_FALSE(Core_Loc_ParseLine("", p));
}

TEST_CASE("loc: labels are cleaned and capped")
{
	CHECK(Core_Loc_CleanLabel("  a\x01\x02 b\x7f\tc  ") == "a b c");
	const std::string long_label(200, 'x');
	CHECK(Core_Loc_CleanLabel(long_label).size() == kLocMaxLabel);
	CHECK(Core_Loc_CleanLabel(std::string(63, 'x') + " y").size() <= kLocMaxLabel);
}

TEST_CASE("loc: a file skips comments and blanks and counts what didn't parse")
{
	size_t skipped = 9;
	const auto places = Core_Loc_ParseFile("// q2dm1\r\n# old\n\n0 0 0 Start\r\nrubbish\n8 8 8 Next", skipped);
	REQUIRE(places.size() == 2);
	CHECK(places[0].label == "Start");
	CHECK(places[1].label == "Next");
	CHECK(skipped == 1);

	std::string big;
	for (size_t i = 0; i < kLocMaxPlaces + 10; ++i)
		big += "0 0 0 p\n";
	CHECK(Core_Loc_ParseFile(big, skipped).size() == kLocMaxPlaces);
}

TEST_CASE("loc: the nearest place in sight, else the nearest")
{
	const std::vector<LocPlace> places = {{{0, 0, 0}, "Hall"}, {{100, 0, 0}, "Lift"}, {{30, 0, 0}, "Behind the wall"}};
	auto none = [](const LocPlace &) { return false; };
	auto all = [](const LocPlace &) { return true; };
	auto not_wall = [](const LocPlace &p) { return p.label != "Behind the wall"; };
	CHECK(Core_Loc_Pick(places, {25, 0, 0}, all) == 2);
	CHECK(Core_Loc_Pick(places, {25, 0, 0}, not_wall) == 0);
	CHECK(Core_Loc_Pick(places, {25, 0, 0}, none) == 2); // nothing in sight: the nearest
	CHECK(Core_Loc_Pick({}, {0, 0, 0}, all) == -1);

	// Only the kLocVisibleTries nearest are traced.
	std::vector<LocPlace> many;
	for (size_t i = 0; i < kLocVisibleTries + 5; ++i)
		many.push_back({{static_cast<float>(i), 0, 0}, "p" + std::to_string(i)});
	int traced = 0;
	auto count = [&](const LocPlace &)
	{
		++traced;
		return false;
	};
	CHECK(Core_Loc_Pick(many, {0, 0, 0}, count) == 0);
	CHECK(traced == static_cast<int>(kLocVisibleTries));
}

TEST_CASE("loc: item names and the upper / lower twins")
{
	CHECK(Core_Loc_ItemName("weapon_railgun") == "RG");
	CHECK(Core_Loc_ItemName("item_health_mega") == "MEGA");
	CHECK(Core_Loc_ItemName("weapon_blaster") == "BLASTER");
	CHECK(Core_Loc_ItemName("ammo_slugs") == "SLUGS");
	CHECK(Core_Loc_ItemName("ammo_grenades") == "GRENADES");
	CHECK(Core_Loc_ItemName("x_") == "?");
	CHECK(Core_Loc_ItemLabel("RG", 100, {}) == "RG");
	CHECK(Core_Loc_ItemLabel("RG", 100, {0}) == "upper RG");
	CHECK(Core_Loc_ItemLabel("RG", 0, {100}) == "lower RG");
	CHECK(Core_Loc_ItemLabel("RG", 50, {0, 100}) == "RG"); // the middle of three: neither
	CHECK(Core_Loc_ItemLabel("RG", 200, {0, 100}) == "upper RG");
	CHECK(Core_Loc_ItemLabel("RG", 0, {0}) == "RG"); // level with it
}

TEST_CASE("loc: armor, names in view, the view cone")
{
	CHECK(Core_Loc_ArmorText(50, -1) == "A:50");
	CHECK(Core_Loc_ArmorText(0, 80) == "A:0 P:80");
	CHECK(Core_Loc_Names({}) == "nobody");
	CHECK(Core_Loc_Names({"Bob", "Carol"}) == "Bob, Carol");
	CHECK(Core_Loc_Names({"a", "b", "c", "d", "e", "f"}) == "a, b, c, d +2");
	CHECK(Core_Loc_InView({0, 0, 0}, {1, 0, 0}, {100, 10, 0}));
	CHECK_FALSE(Core_Loc_InView({0, 0, 0}, {1, 0, 0}, {-100, 0, 0})); // behind
	CHECK_FALSE(Core_Loc_InView({0, 0, 0}, {1, 0, 0}, {0, 100, 0}));  // beside
}

TEST_CASE("loc: the text needs a token; quotes and space are trimmed")
{
	CHECK(Core_Loc_Text("ENEMY - %l") == "ENEMY - %l");
	CHECK(Core_Loc_Text("  \"STATUS %h %a\"  ") == "STATUS %h %a");
	CHECK(Core_Loc_Text("hello") == "");
	CHECK(Core_Loc_Text("100%") == "");
	CHECK(Core_Loc_Text("") == "");
	CHECK(Core_Loc_Uses("a %n b", 'n'));
	CHECK_FALSE(Core_Loc_Uses("a %N b", 'n'));
}

TEST_CASE("loc: MuffMode's binds expand as they did")
{
	LocValues v;
	v.place = "Red Armor";
	v.health = 87;
	v.armor = "A:50";
	v.weapon = "RG:25";
	v.ammo = "SLUGS";
	v.team = "Bob";
	v.all = "Bob, Carol";
	CHECK(Core_Loc_Expand("NEED WEAPON - %l", v) == "NEED WEAPON - [Red Armor]");
	CHECK(Core_Loc_Expand("STATUS %h %a %l", v) == "STATUS H:87 A:50 [Red Armor]");
	CHECK(Core_Loc_Expand("%L %w %m", v) == "[Red Armor] RG:25 SLUGS");
	CHECK(Core_Loc_Expand("with %n, see %N", v) == "with Bob, see Bob, Carol");
	CHECK(Core_Loc_Expand("100% sure %x %", v) == "100% sure %x %");
	CHECK(Core_Loc_Expand("%%l", v) == "%[Red Armor]");

	// One pass: a place or name holding a token isn't expanded again.
	v.place = "%h room";
	CHECK(Core_Loc_Expand("at %l", v) == "at [%h room]");

	std::string long_text(300, 'x');
	CHECK(Core_Loc_Expand(long_text + " %l", v).size() == kLocMaxText);
}
