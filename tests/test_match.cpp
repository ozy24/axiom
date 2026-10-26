#include "doctest.h"

#include "core/match.h"

#include <algorithm>

namespace
{
MatchPlayer P(int id, bool ready = false, int score = 0, bool playing = true)
{
	MatchPlayer p;
	p.id = id;
	p.playing = playing;
	p.ready = ready;
	p.score = score;
	return p;
}

MatchInput At(int64_t now_ms, std::vector<MatchPlayer> players, bool over = false)
{
	MatchInput in;
	in.now_ms = now_ms;
	in.players = std::move(players);
	in.gametype_over = over;
	return in;
}

bool Has(const std::vector<MatchAction> &out, MatchActionKind kind)
{
	return std::any_of(out.begin(), out.end(), [&](const MatchAction &a) { return a.kind == kind; });
}

MatchConfig Enabled()
{
	MatchConfig c;
	c.enabled = true;
	c.countdown_ms = 3000;
	c.timelimit_ms = 60000;
	return c;
}

// Ready two players and run the countdown out; returns the time LIVE began.
int64_t ToLive(Match &m, int64_t t = 1000)
{
	m.Begin(0);
	m.Update(At(t, {P(1, true), P(2, true)}));
	auto out = m.Update(At(t + 3000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Live);
	REQUIRE(Has(out, MatchActionKind::ResetArena));
	return t + 3000;
}
} // namespace

TEST_CASE("disabled (the default) is stock: live at once, never acts")
{
	Match m;
	m.Begin(0);
	CHECK(m.State() == MatchState::Live);
	CHECK_FALSE(m.OwnsRules());
	CHECK(m.Update(At(999999, {P(1, false, 500), P(2, false, 500)})).empty());
	CHECK(m.State() == MatchState::Live);
}

TEST_CASE("warmup waits until every playing client is ready")
{
	Match m(Enabled());
	m.Begin(0);
	CHECK(m.State() == MatchState::Warmup);
	CHECK(m.OwnsRules());
	CHECK(m.Update(At(100, {P(1, true), P(2, false)})).empty());
	CHECK(m.State() == MatchState::Warmup);
	// a spectator doesn't need to ready
	auto out = m.Update(At(200, {P(1, true), P(2, true), P(3, false, 0, false)}));
	CHECK(m.State() == MatchState::Countdown);
	CHECK(Has(out, MatchActionKind::Countdown));
}

TEST_CASE("warmup needs match_minplayers")
{
	Match m(Enabled());
	m.Begin(0);
	m.Update(At(100, {P(1, true)}));
	CHECK(m.State() == MatchState::Warmup);
	MatchConfig solo = Enabled();
	solo.min_players = 1;
	Match one(solo);
	one.Begin(0);
	one.Update(At(100, {P(1, true)}));
	CHECK(one.State() == MatchState::Countdown);
}

TEST_CASE("the countdown announces each second once, then goes live with a reset")
{
	Match m(Enabled());
	m.Begin(0);
	std::vector<int> seconds;
	auto collect = [&](const std::vector<MatchAction> &out)
	{
		for (const MatchAction &a : out)
			if (a.kind == MatchActionKind::Countdown)
				seconds.push_back(a.value);
	};
	collect(m.Update(At(1000, {P(1, true), P(2, true)})));
	for (int64_t t = 1100; t < 4000; t += 100)
		collect(m.Update(At(t, {P(1, true), P(2, true)})));
	CHECK(seconds == std::vector<int>{3, 2, 1});
	auto out = m.Update(At(4000, {P(1, true), P(2, true)}));
	CHECK(m.State() == MatchState::Live);
	CHECK(Has(out, MatchActionKind::ResetArena));
	CHECK(m.LiveSinceMs() == 4000);
}

TEST_CASE("un-readying or leaving cancels the countdown")
{
	Match m(Enabled());
	m.Begin(0);
	m.Update(At(1000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Countdown);
	auto out = m.Update(At(1500, {P(1, true), P(2, false)}));
	CHECK(m.State() == MatchState::Warmup);
	CHECK(Has(out, MatchActionKind::Announce));
	m.Update(At(2000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Countdown);
	m.Update(At(2500, {P(1, true)})); // player 2 left
	CHECK(m.State() == MatchState::Warmup);
}

TEST_CASE("match_warmup_time starts the match without everyone ready")
{
	MatchConfig c = Enabled();
	c.warmup_time_ms = 30000;
	Match m(c);
	m.Begin(0);
	m.Update(At(29999, {P(1, false), P(2, false)}));
	CHECK(m.State() == MatchState::Warmup);
	m.Update(At(30000, {P(1, false), P(2, false)}));
	CHECK(m.State() == MatchState::Countdown);
	// and isn't cancelled for lack of ready
	m.Update(At(31000, {P(1, false), P(2, false)}));
	CHECK(m.State() == MatchState::Countdown);
}

TEST_CASE("match_ready 0 starts as soon as enough players are in")
{
	MatchConfig c = Enabled();
	c.ready_required = false;
	Match m(c);
	m.Begin(0);
	m.Update(At(10, {P(1), P(2)}));
	CHECK(m.State() == MatchState::Countdown);
}

TEST_CASE("timelimit counts from the start of LIVE, not map load")
{
	Match m(Enabled());
	const int64_t live = ToLive(m);
	m.Update(At(live + 59999, {P(1, true, 3), P(2, true, 1)}));
	CHECK(m.State() == MatchState::Live);
	auto out = m.Update(At(live + 60000, {P(1, true, 3), P(2, true, 1)}));
	CHECK(m.State() == MatchState::Intermission);
	CHECK(Has(out, MatchActionKind::EndMatch));
}

TEST_CASE("fraglimit ends the match")
{
	MatchConfig c = Enabled();
	c.fraglimit = 10;
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 9), P(2, true, 2)}));
	CHECK(m.State() == MatchState::Live);
	m.Update(At(live + 200, {P(1, true, 9), P(2, true, 10)}));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("a tie at the timelimit with no overtime just ends (stock)")
{
	Match m(Enabled());
	const int64_t live = ToLive(m);
	m.Update(At(live + 60000, {P(1, true, 4), P(2, true, 4)}));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("sudden death: a tie goes to overtime, the next score wins")
{
	MatchConfig c = Enabled();
	c.overtime = -1;
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 60000, {P(1, true, 4), P(2, true, 4)}));
	REQUIRE(m.State() == MatchState::Overtime);
	m.Update(At(live + 90000, {P(1, true, 4), P(2, true, 4)}));
	CHECK(m.State() == MatchState::Overtime); // no time limit in sudden death
	m.Update(At(live + 95000, {P(1, true, 5), P(2, true, 4)}));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("timed overtime repeats while still tied")
{
	MatchConfig c = Enabled();
	c.overtime = 2; // minutes
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 60000, {P(1, true, 4), P(2, true, 4)}));
	REQUIRE(m.State() == MatchState::Overtime);
	m.Update(At(live + 60000 + 120000, {P(1, true, 5), P(2, true, 5)}));
	CHECK(m.State() == MatchState::Overtime); // still tied: another 2 minutes
	m.Update(At(live + 60000 + 240000, {P(1, true, 6), P(2, true, 5)}));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("the gametype's own end condition ends LIVE and OVERTIME")
{
	Match m(Enabled());
	const int64_t live = ToLive(m);
	m.Update(At(live + 10, {P(1, true), P(2, true)}, true));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("a top score held by the leader alone is not a tie")
{
	MatchConfig c = Enabled();
	c.overtime = -1;
	Match m(c);
	const int64_t live = ToLive(m);
	// the higher score is listed second: ordering must not matter
	m.Update(At(live + 60000, {P(1, true, 2), P(2, true, 7), P(3, true, 2)}));
	CHECK(m.State() == MatchState::Intermission);
}
