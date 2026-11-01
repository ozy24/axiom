#include "doctest.h"

#include "core/timeout.h"

#include <algorithm>

namespace
{
TimeoutConfig On(int64_t length_ms = 120000, int64_t resume_ms = 10000)
{
	TimeoutConfig c;
	c.length_ms = length_ms;
	c.resume_ms = resume_ms;
	return c;
}

bool Says(const std::vector<MatchAction> &out, MatchActionKind kind, const std::string &text)
{
	return std::any_of(out.begin(), out.end(), [&](const MatchAction &a) { return a.kind == kind && a.text == text; });
}

// Ticks 100 ms frames until the timeout is over (or `max` frames); every action.
std::vector<MatchAction> RunOut(Timeout &t, int max = 100000)
{
	std::vector<MatchAction> all;
	for (int i = 0; i < max && t.Paused(); ++i)
	{
		const std::vector<MatchAction> out = t.Tick(100);
		all.insert(all.end(), out.begin(), out.end());
	}
	return all;
}
} // namespace

TEST_CASE("timeout: off, outside play, or by a spectator is refused")
{
	Timeout t;
	CHECK(t.Call("red", true, true, On(0)) == TimeoutError::Off);
	CHECK(t.Call("red", false, true, On()) == TimeoutError::NotInPlay);
	CHECK(t.Call("red", true, false, On()) == TimeoutError::NotPlaying);
	CHECK_FALSE(t.Paused());
	// None of those used the side's timeout.
	CHECK(t.Call("red", true, true, On()) == TimeoutError::None);
	CHECK(t.Paused());
}

TEST_CASE("timeout: one per side per match, one at a time")
{
	Timeout t;
	REQUIRE(t.Call("red", true, true, On()) == TimeoutError::None);
	CHECK(t.Side() == "red");
	CHECK(t.Call("blue", true, true, On()) == TimeoutError::Running);
	RunOut(t);
	CHECK_FALSE(t.Paused());
	CHECK(t.Call("red", true, true, On()) == TimeoutError::Used);
	CHECK(t.Call("blue", true, true, On()) == TimeoutError::None); // blue's is still there
	RunOut(t);
	CHECK(t.Call("blue", true, true, On()) == TimeoutError::Used);

	// A new match gives every side its timeout back.
	t.NewMatch();
	CHECK(t.Call("red", true, true, On()) == TimeoutError::None);
}

TEST_CASE("timeout: runs its length, then the resume countdown, then FIGHT")
{
	Timeout t;
	REQUIRE(t.Call("id:alice", true, true, On(2000, 3000)) == TimeoutError::None);
	CHECK(t.Status() == "TIMEOUT 0:02");
	CHECK(t.CountdownSeconds() == 0);
	for (int i = 0; i < 20; ++i)
		CHECK(t.Tick(100).empty());
	CHECK(t.Phase() == TimeoutPhase::Paused);
	std::vector<MatchAction> out = t.Tick(100); // the length has run out
	CHECK(t.Phase() == TimeoutPhase::Resuming);
	CHECK(Says(out, MatchActionKind::Announce, "Match resumes"));
	CHECK(t.Status() == "RESUMING");
	CHECK(t.CountdownSeconds() == 3);
	out = RunOut(t);
	CHECK_FALSE(t.Paused());
	CHECK(Says(out, MatchActionKind::Sound, "world/3.wav"));
	CHECK(Says(out, MatchActionKind::Sound, "world/2.wav"));
	CHECK(Says(out, MatchActionKind::Sound, "world/1.wav"));
	CHECK(Says(out, MatchActionKind::Announce, "FIGHT!"));
	CHECK(Says(out, MatchActionKind::Sound, "misc/tele_up.wav"));
	CHECK(t.Status().empty());
}

TEST_CASE("timeout: each second of the resume countdown is spoken once")
{
	Timeout t;
	REQUIRE(t.Call("red", true, true, On(100, 10000)) == TimeoutError::None);
	const std::vector<MatchAction> out = RunOut(t);
	for (int s = 1; s <= 10; ++s)
	{
		const std::string sound = "world/" + std::to_string(s) + ".wav";
		CHECK(std::count_if(out.begin(), out.end(), [&](const MatchAction &a) { return a.text == sound; }) == 1);
	}
}

TEST_CASE("timeout: no resume countdown resumes at once")
{
	Timeout t;
	REQUIRE(t.Call("red", true, true, On(200, 0)) == TimeoutError::None);
	const std::vector<MatchAction> out = RunOut(t);
	CHECK_FALSE(t.Paused());
	CHECK(Says(out, MatchActionKind::Announce, "FIGHT!"));
	CHECK_FALSE(Says(out, MatchActionKind::Announce, "Match resumes"));
}

TEST_CASE("timeout: only the calling side or the server ends it early")
{
	Timeout t;
	CHECK(t.TimeIn("red") == TimeoutError::NotRunning);
	REQUIRE(t.Call("red", true, true, On()) == TimeoutError::None);
	CHECK(t.TimeIn("blue") == TimeoutError::NotYours);
	CHECK(t.TimeIn("red") == TimeoutError::None);
	t.Tick(100);
	CHECK(t.Phase() == TimeoutPhase::Resuming);
	CHECK(t.TimeIn("red") == TimeoutError::Resuming);

	Timeout s;
	REQUIRE(s.Call("red", true, true, On()) == TimeoutError::None);
	CHECK(s.TimeIn(Timeout::kServer) == TimeoutError::None);
	s.Tick(100);
	CHECK(s.Phase() == TimeoutPhase::Resuming);
}

TEST_CASE("timeout: the server's needs no allowance, and only it ends one")
{
	Timeout t;
	REQUIRE(t.Call(Timeout::kServer, true, false, On(0)) == TimeoutError::None); // even with timeouts off
	CHECK(t.LeftMs() == 120000);
	CHECK(t.TimeIn("red") == TimeoutError::NotYours);
	CHECK(t.TimeIn(Timeout::kServer) == TimeoutError::None);
	RunOut(t);
	CHECK(t.Call(Timeout::kServer, true, false, On()) == TimeoutError::None); // and again
	CHECK(t.Call(Timeout::kServer, true, false, On()) == TimeoutError::Running);
	t.Cancel();
	CHECK(t.Call(Timeout::kServer, false, false, On()) == TimeoutError::NotInPlay);
}

TEST_CASE("timeout: cancelling keeps the allowance; a new match restores it")
{
	Timeout t;
	REQUIRE(t.Call("red", true, true, On()) == TimeoutError::None);
	t.Cancel();
	CHECK_FALSE(t.Paused());
	CHECK(t.Used("red"));
	t.NewMatch();
	CHECK_FALSE(t.Used("red"));
}

TEST_CASE("timeout: every refusal has words")
{
	for (TimeoutError e :
	     {TimeoutError::Off, TimeoutError::NotInPlay, TimeoutError::NotPlaying, TimeoutError::Used,
	      TimeoutError::Running, TimeoutError::NotRunning, TimeoutError::NotYours, TimeoutError::Resuming})
		CHECK_FALSE(Core_Timeout_ErrorText(e).empty());
	CHECK(Core_Timeout_ErrorText(TimeoutError::None).empty());
}
