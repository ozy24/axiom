#include "core/eyecam.h"

#include "doctest.h"

TEST_CASE("eyecam: first person by default, flipped per client, reset when they leave")
{
	EyeCamPrefs prefs;
	CHECK(prefs.FirstPerson(0));
	CHECK(prefs.FirstPerson(5));
	CHECK_FALSE(prefs.Toggle(5)); // now third person
	CHECK_FALSE(prefs.FirstPerson(5));
	CHECK(prefs.FirstPerson(0)); // others unchanged
	CHECK(prefs.Toggle(5));
	CHECK(prefs.FirstPerson(5));
	prefs.Toggle(7);
	prefs.Forget(7); // the next client in slot 7 starts in first person
	CHECK(prefs.FirstPerson(7));
	CHECK(prefs.FirstPerson(-1)); // out of range is harmless
	CHECK(prefs.Toggle(EyeCamPrefs::kSlots));
}
