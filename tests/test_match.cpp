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

// --- Rounds (HoonyMode) ------------------------------------------------------

namespace
{
MatchConfig Hoony()
{
	MatchConfig c = Enabled();
	c.rounds_on_frag = true;
	c.round_countdown_ms = 2000;
	c.fraglimit = 3;
	return c;
}

bool HasReset(const std::vector<MatchAction> &out, int value)
{
	return std::any_of(out.begin(), out.end(),
	                   [&](const MatchAction &a) { return a.kind == MatchActionKind::ResetArena && a.value == value; });
}
} // namespace

TEST_CASE("a frag ends the round: hold, level-only reset, countdown, next round")
{
	Match m(Hoony());
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)})); // baseline
	auto out = m.Update(At(live + 5000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	CHECK(Has(out, MatchActionKind::RoundHold));
	CHECK(HasReset(out, 1));
	CHECK_FALSE(HasReset(out, 0)); // scores are kept between rounds
	out = m.Update(At(live + 5100, {P(1, true, 1), P(2, true, 0)}));
	CHECK(Has(out, MatchActionKind::Countdown));
	out = m.Update(At(live + 7000, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Live);
	CHECK(Has(out, MatchActionKind::RoundStart));
	// the new round's baseline is 1-0: no break until the score moves again
	m.Update(At(live + 7100, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Live);
	m.Update(At(live + 9000, {P(1, true, 1), P(2, true, 1)}));
	CHECK(m.State() == MatchState::RoundCountdown);
}

TEST_CASE("the deciding frag ends the match instead of starting a round")
{
	Match m(Hoony());
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 2), P(2, true, 0)}));
	m.Update(At(live + 3000, {P(1, true, 2), P(2, true, 0)}));
	auto out = m.Update(At(live + 4000, {P(1, true, 3), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Intermission);
	CHECK(Has(out, MatchActionKind::EndMatch));
	CHECK_FALSE(Has(out, MatchActionKind::RoundHold));
}

TEST_CASE("a duellist leaving between rounds is still a forfeit")
{
	Match m(Hoony());
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)}));
	m.Update(At(live + 500, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	m.Update(At(live + 600, {P(1, true, 1)}, true));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("rounds off (the default): a frag is just a frag")
{
	Match m(Enabled());
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)}));
	m.Update(At(live + 500, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Live);
}

TEST_CASE("sudden death with rounds still ends on the first frag")
{
	MatchConfig c = Hoony();
	c.fraglimit = 0;
	c.overtime = -1;
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 2), P(2, true, 2)}));
	m.Update(At(live + 60000, {P(1, true, 2), P(2, true, 2)}));
	REQUIRE(m.State() == MatchState::Overtime);
	m.Update(At(live + 61000, {P(1, true, 3), P(2, true, 2)}));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("timed overtime keeps its rounds and returns to overtime")
{
	MatchConfig c = Hoony();
	c.fraglimit = 0;
	c.overtime = 2;
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 1), P(2, true, 1)}));
	m.Update(At(live + 60000, {P(1, true, 1), P(2, true, 1)}));
	REQUIRE(m.State() == MatchState::Overtime);
	m.Update(At(live + 61000, {P(1, true, 2), P(2, true, 1)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	m.Update(At(live + 63000, {P(1, true, 2), P(2, true, 1)}));
	CHECK(m.State() == MatchState::Overtime);
}

// --- Teams (TDM, CTF) ----------------------------------------------------------

namespace
{
MatchPlayer T(int id, int team, int score = 0)
{
	MatchPlayer p = P(id, true, score);
	p.team = team;
	return p;
}

MatchInput Teams(int64_t now_ms, std::vector<MatchPlayer> players, int score1, int score2)
{
	MatchInput in = At(now_ms, std::move(players));
	in.team_scores = {score1, score2};
	return in;
}

MatchConfig TeamConfig()
{
	MatchConfig c = Enabled();
	c.teams = true;
	return c;
}

int64_t TeamsToLive(Match &m)
{
	m.Begin(0);
	m.Update(Teams(1000, {T(1, 1), T(2, 2)}, 0, 0));
	const int64_t live = 1000 + m.Config().countdown_ms;
	m.Update(Teams(live, {T(1, 1), T(2, 2)}, 0, 0));
	REQUIRE(m.State() == MatchState::Live);
	return live;
}
} // namespace

TEST_CASE("teams: the match waits for someone on each team")
{
	Match m(TeamConfig());
	m.Begin(0);
	m.Update(Teams(1000, {T(1, 1), T(2, 1)}, 0, 0));
	CHECK(m.State() == MatchState::Warmup); // two players, one team
	m.Update(Teams(2000, {T(1, 1), T(2, 1), T(3, 0)}, 0, 0));
	CHECK(m.State() == MatchState::Warmup); // on no team doesn't count
	m.Update(Teams(3000, {T(1, 1), T(2, 2)}, 0, 0));
	CHECK(m.State() == MatchState::Countdown);
	m.Update(Teams(3500, {T(1, 1)}, 0, 0));
	CHECK(m.State() == MatchState::Warmup); // a team emptied: cancelled
}

TEST_CASE("teams: a tie is between team scores, not players")
{
	MatchConfig c = TeamConfig();
	c.overtime = -1;
	Match m(c);
	const int64_t live = TeamsToLive(m);
	// players tied 3-3, but team 1 leads
	m.Update(Teams(live + 60000, {T(1, 1, 3), T(2, 2, 3)}, 2, 1));
	CHECK(m.State() == MatchState::Intermission);

	Match tied(c);
	const int64_t live2 = TeamsToLive(tied);
	// players apart, teams level: overtime, then the team that scores wins
	tied.Update(Teams(live2 + 60000, {T(1, 1, 5), T(2, 2, 1)}, 3, 3));
	REQUIRE(tied.State() == MatchState::Overtime);
	tied.Update(Teams(live2 + 61000, {T(1, 1, 6), T(2, 2, 1)}, 3, 3));
	CHECK(tied.State() == MatchState::Overtime); // a frag that doesn't change the team score
	tied.Update(Teams(live2 + 62000, {T(1, 1, 6), T(2, 2, 1)}, 4, 3));
	CHECK(tied.State() == MatchState::Intermission);
}

TEST_CASE("teams: fraglimit is per player unless it applies to teams (TDM)")
{
	MatchConfig c = TeamConfig();
	c.fraglimit = 10;
	Match ctf(c);
	const int64_t live = TeamsToLive(ctf);
	ctf.Update(Teams(live + 100, {T(1, 1, 6), T(2, 1, 6)}, 12, 0)); // team 12, nobody 10
	CHECK(ctf.State() == MatchState::Live);
	ctf.Update(Teams(live + 200, {T(1, 1, 10), T(2, 1, 6)}, 16, 0));
	CHECK(ctf.State() == MatchState::Intermission);

	c.fraglimit_on_teams = true;
	Match tdm(c);
	const int64_t live2 = TeamsToLive(tdm);
	tdm.Update(Teams(live2 + 100, {T(1, 1, 6), T(2, 1, 3), T(3, 2, 1)}, 9, 1));
	CHECK(tdm.State() == MatchState::Live);
	tdm.Update(Teams(live2 + 200, {T(1, 1, 6), T(2, 1, 4), T(3, 2, 1)}, 10, 1));
	CHECK(tdm.State() == MatchState::Intermission);
}

TEST_CASE("the gametype's end condition carries its reason (capturelimit)")
{
	Match m(TeamConfig());
	const int64_t live = TeamsToLive(m);
	MatchInput in = Teams(live + 100, {T(1, 1), T(2, 2)}, 8, 2);
	in.gametype_over = true;
	in.gametype_over_reason = "capturelimit";
	auto out = m.Update(in);
	REQUIRE(m.State() == MatchState::Intermission);
	CHECK(std::any_of(out.begin(), out.end(), [](const MatchAction &a)
	                  { return a.kind == MatchActionKind::EndMatch && a.text == "capturelimit"; }));
}

// --- Free play before LIVE -----------------------------------------------------

TEST_CASE("scores only count from LIVE; the clock only runs from LIVE")
{
	MatchConfig c = Enabled();
	c.overtime = -1;
	Match m(c);
	m.Begin(0);
	CHECK_FALSE(m.ScoresCount()); // warmup
	CHECK(m.ClockEndMs() == 0);
	m.Update(At(1000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Countdown);
	CHECK_FALSE(m.ScoresCount());
	CHECK(m.ClockEndMs() == 0);
	m.Update(At(4000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(m.ScoresCount());
	CHECK(m.ClockEndMs() == 4000 + 60000); // the timelimit, from LIVE
	m.Update(At(64000, {P(1, true, 2), P(2, true, 2)}));
	REQUIRE(m.State() == MatchState::Overtime);
	CHECK(m.ScoresCount());
	CHECK(m.ClockEndMs() == 0); // sudden death has no clock

	Match stock; // match flow off: stock scoring
	stock.Begin(0);
	CHECK(stock.ScoresCount());
}
