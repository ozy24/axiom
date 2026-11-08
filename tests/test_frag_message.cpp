#include "core/frag_message.h"

#include "doctest.h"

TEST_CASE("frag messages: the place among everyone playing, ties included")
{
	CHECK(Core_FragPlace({12, 7, 3}, 12).place == 1);
	CHECK_FALSE(Core_FragPlace({12, 7, 3}, 12).tied);
	CHECK(Core_FragPlace({12, 7, 3}, 7).place == 2);
	const FragPlace tied = Core_FragPlace({9, 9, 3}, 9);
	CHECK(tied.place == 1);
	CHECK(tied.tied);
	CHECK(Core_FragPlace({9, 9, 3}, 3).place == 3); // two above: third, as Quake III counts
	CHECK(Core_FragPlace({5}, 5).place == 1);
}

TEST_CASE("frag messages: place names")
{
	CHECK(Core_FragPlaceText({1, false}) == "1st");
	CHECK(Core_FragPlaceText({2, false}) == "2nd");
	CHECK(Core_FragPlaceText({3, false}) == "3rd");
	CHECK(Core_FragPlaceText({4, false}) == "4th");
	CHECK(Core_FragPlaceText({11, false}) == "11th");
	CHECK(Core_FragPlaceText({12, false}) == "12th");
	CHECK(Core_FragPlaceText({13, false}) == "13th");
	CHECK(Core_FragPlaceText({21, false}) == "21st");
	CHECK(Core_FragPlaceText({22, false}) == "22nd");
	CHECK(Core_FragPlaceText({1, true}) == "Tied for 1st");
}

TEST_CASE("frag messages: the killer's and the victim's text, the name left to the client")
{
	CHECK(Core_FragKillerText(false, false, {}, 0) == "You fragged {}");
	CHECK(Core_FragKillerText(true, false, {}, 0) == "You fragged {}, your teammate :(");
	CHECK(Core_FragKillerText(true, true, {}, 0) == "You fragged {}, your teammate :(");
	CHECK(Core_FragKillerText(false, true, {2, false}, 12) == "You fragged {}\n2nd place with 12");
	CHECK(Core_FragKillerText(false, true, {1, true}, 9) == "You fragged {}\nTied for 1st place with 9");
	CHECK(Core_FragVictimText() == "You were fragged by {}");
}
