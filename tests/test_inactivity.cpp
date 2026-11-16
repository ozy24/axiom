#include "doctest.h"

#include "core/inactivity.h"

TEST_CASE("inactivity: warned 10 s before, moved at the limit, once each")
{
	Inactivity in;
	in.Reset(3, 0);
	CHECK(in.Frame(3, 50000, false, 120) == InactivityEvent::None);
	CHECK(in.Frame(3, 110000, false, 120) == InactivityEvent::Warn);
	CHECK(in.Frame(3, 111000, false, 120) == InactivityEvent::None); // warned once
	CHECK(in.Frame(3, 120000, false, 120) == InactivityEvent::Move);
	CHECK(in.Frame(3, 120100, false, 120) == InactivityEvent::None); // the clock starts again
}

TEST_CASE("inactivity: any input starts the clock again, the warning too")
{
	Inactivity in;
	in.Reset(0, 0);
	CHECK(in.Frame(0, 112000, false, 120) == InactivityEvent::Warn);
	CHECK(in.Frame(0, 113000, true, 120) == InactivityEvent::None);
	CHECK(in.Frame(0, 200000, false, 120) == InactivityEvent::None); // 87 s idle
	CHECK(in.Frame(0, 223000, false, 120) == InactivityEvent::Warn);
	CHECK(in.Frame(0, 233000, false, 120) == InactivityEvent::Move);
}

TEST_CASE("inactivity: off, an unstarted slot, a new level and bad slots never move anyone")
{
	Inactivity in;
	CHECK(in.Frame(1, 500000, false, 120) == InactivityEvent::None);        // starts the clock
	CHECK(in.Frame(1, 500000 + 119000, false, 0) == InactivityEvent::None); // off
	in.Reset(2, 900000);
	CHECK(in.Frame(2, 1000, false, 120) == InactivityEvent::None); // time went back: a new level
	CHECK(in.Frame(-1, 999999, false, 120) == InactivityEvent::None);
	CHECK(in.Frame(Inactivity::kSlots, 999999, false, 120) == InactivityEvent::None);
}

TEST_CASE("inactivity: the setting's floor")
{
	CHECK(Inactivity::Seconds(0) == 0);
	CHECK(Inactivity::Seconds(-5) == 0);
	CHECK(Inactivity::Seconds(5) == 15);
	CHECK(Inactivity::Seconds(120) == 120);
}

TEST_CASE("inactivity: buttons, movement and a real turn are input; jitter isn't")
{
	const float a[2] = {10.0f, 90.0f}, same[2] = {10.0f, 90.0f}, jitter[2] = {10.4f, 90.6f}, turned[2] = {10.0f, 95.0f},
	            wrapped[2] = {10.0f, -269.5f};
	CHECK_FALSE(Core_Inactivity_Active(0, 0, 0, 0, same, a));
	CHECK_FALSE(Core_Inactivity_Active(0, 0, 0, 0, jitter, a));
	CHECK_FALSE(Core_Inactivity_Active(0, 0, 0, 0, wrapped, a)); // 90.5 and -269.5 are the same way
	CHECK(Core_Inactivity_Active(0, 0, 0, 0, turned, a));
	CHECK(Core_Inactivity_Active(1, 0, 0, 0, same, a));
	CHECK(Core_Inactivity_Active(0, 400, 0, 0, same, a));
	CHECK(Core_Inactivity_Active(0, 0, -200, 0, same, a));
	CHECK(Core_Inactivity_Active(0, 0, 0, 200, same, a));
}
