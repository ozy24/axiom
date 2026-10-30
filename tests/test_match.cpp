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

TEST_CASE("a new level waits for players still loading it before the countdown")
{
	MatchConfig c = Enabled();
	c.join_grace_ms = 10000;
	c.join_wait_ms = 30000;
	Match m(c);
	m.Begin(0);
	// The bots are back and ready; a human is still loading the map.
	MatchPlayer loading = P(3, false, 0, false);
	loading.joining = true;
	CHECK(m.Update(At(500, {P(1, true), P(2, true)})).empty()); // within the grace
	CHECK(m.Update(At(11000, {P(1, true), P(2, true), loading})).empty());
	CHECK(m.State() == MatchState::Warmup);
	// In, unready: still warmup, nothing to cancel.
	CHECK(m.Update(At(12000, {P(1, true), P(2, true), P(3, false)})).empty());
	CHECK(m.State() == MatchState::Warmup);
	m.Update(At(13000, {P(1, true), P(2, true), P(3, true)}));
	CHECK(m.State() == MatchState::Countdown);

	// One stuck loading holds the start only so long.
	Match stuck(c);
	stuck.Begin(0);
	CHECK(stuck.Update(At(29000, {P(1, true), P(2, true), loading})).empty());
	stuck.Update(At(30000, {P(1, true), P(2, true), loading}));
	CHECK(stuck.State() == MatchState::Countdown);
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
	c.rounds = RoundsMode::Frag;
	c.round_pause_ms = 1000;
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

TEST_CASE("a frag ends the round: a pause, a frozen respawn and countdown, the next round")
{
	Match m(Hoony());
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)})); // baseline
	auto out = m.Update(At(live + 5000, {P(1, true, 1), P(2, true, 0)}));
	// The round is over: the dead stay dead, the winner plays on, nothing reset yet.
	REQUIRE(m.State() == MatchState::RoundOver);
	CHECK(Has(out, MatchActionKind::RoundHold));
	CHECK_FALSE(Has(out, MatchActionKind::ResetArena));
	CHECK_FALSE(m.Frozen());
	out = m.Update(At(live + 5900, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::RoundOver);
	// After the pause: items back, everyone respawned, frozen for the countdown.
	out = m.Update(At(live + 6000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	CHECK(HasReset(out, 1));
	CHECK_FALSE(HasReset(out, 0)); // scores are kept between rounds
	CHECK(Has(out, MatchActionKind::RoundRespawn));
	CHECK(m.Frozen());
	out = m.Update(At(live + 6100, {P(1, true, 1), P(2, true, 0)}));
	CHECK(Has(out, MatchActionKind::Countdown));
	out = m.Update(At(live + 8000, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Live);
	CHECK(Has(out, MatchActionKind::RoundStart));
	CHECK_FALSE(m.Frozen());
	// the new round's baseline is 1-0: no break until the score moves again
	m.Update(At(live + 8100, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Live);
	m.Update(At(live + 9000, {P(1, true, 1), P(2, true, 1)}));
	CHECK(m.State() == MatchState::RoundOver);
}

TEST_CASE("time running out during the pause still respawns everyone, then ends")
{
	MatchConfig c = Hoony();
	c.fraglimit = 0;
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)}));
	m.Update(At(live + 59500, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundOver);
	auto out = m.Update(At(live + 60000, {P(1, true, 1), P(2, true, 0)}));
	CHECK(Has(out, MatchActionKind::RoundRespawn)); // nobody left dead
	CHECK(m.State() == MatchState::Live);
	m.Update(At(live + 60100, {P(1, true, 1), P(2, true, 0)}));
	CHECK(m.State() == MatchState::Intermission);
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
	REQUIRE(m.State() == MatchState::RoundOver);
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
	REQUIRE(m.State() == MatchState::RoundOver);
	m.Update(At(live + 62000, {P(1, true, 2), P(2, true, 1)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	m.Update(At(live + 64000, {P(1, true, 2), P(2, true, 1)}));
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

TEST_CASE("a suicide's point goes to the only opponent, or nobody")
{
	CHECK(Core_SuicideOpponent({P(1), P(2)}, 1) == 2);
	CHECK(Core_SuicideOpponent({P(1), P(2), P(3, false, 0, false)}, 2) == 1); // a spectator isn't one
	CHECK(Core_SuicideOpponent({P(1), P(2), P(3)}, 1) == -1);                 // FFA: whose point?
	CHECK(Core_SuicideOpponent({P(1)}, 1) == -1);
}

// --- The HUD's match status and clock ------------------------------------------

namespace
{
MatchHudText Hud(std::string status, std::string clock)
{
	return {std::move(status), std::move(clock)};
}

bool operator==(const MatchHudText &a, const MatchHudText &b)
{
	return a.status == b.status && a.clock == b.clock;
}
} // namespace

TEST_CASE("the HUD: the match length in warmup and the countdown, then the clock")
{
	MatchConfig c = Enabled(); // countdown 3 s, timelimit 60 s
	Match m(c);
	m.Begin(0);
	CHECK(m.HudText(At(500, {P(1, true), P(2, false)})) == Hud("READY UP 1/2", "1:00"));
	CHECK(m.HudText(At(500, {P(1, false)})) == Hud("WAITING FOR PLAYERS", "1:00")); // match_minplayers 2
	CHECK(m.CountdownSeconds(500) == 0);
	m.Update(At(1000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Countdown);
	CHECK(m.HudText(At(1000, {P(1, true), P(2, true)})) == Hud("COUNTDOWN", "1:00"));
	CHECK(m.CountdownSeconds(1000) == 3);
	CHECK(m.CountdownSeconds(3100) == 1);
	m.Update(At(4000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(m.HudText(At(4000, {P(1, true), P(2, true)})) == Hud("", "1:00"));
	CHECK(m.HudText(At(4000 + 18500, {P(1, true), P(2, true)})) == Hud("", "0:42"));
	m.Update(At(64000, {P(1, true, 2), P(2, true, 1)}));
	REQUIRE(m.State() == MatchState::Intermission);
	CHECK(m.HudText(At(64000, {})) == Hud("MATCH END", ""));
}

TEST_CASE("the HUD: warmup says what the start waits for, teams included")
{
	MatchConfig c = Enabled();
	c.teams = true;
	Match m(c);
	m.Begin(0);
	MatchPlayer red = P(1, false), red2 = P(2, false), blue = P(3, false);
	red.team = red2.team = 1;
	blue.team = 2;
	CHECK(m.HudText(At(500, {red})) == Hud("WAITING FOR PLAYERS", "1:00"));
	CHECK(m.HudText(At(500, {red, red2})) == Hud("WAITING FOR TEAMS", "1:00"));
	CHECK(m.HudText(At(500, {red, blue})) == Hud("READY UP 0/2", "1:00"));
}

TEST_CASE("the HUD: ready-up counts the ready players; spectators don't count; no ready-up says WARMUP")
{
	MatchConfig c = Enabled();
	Match m(c);
	m.Begin(0);
	MatchPlayer spectator = P(4, true);
	spectator.playing = false;
	CHECK(m.HudText(At(500, {P(1, true), P(2, false), P(3, true), spectator})) == Hud("READY UP 2/3", "1:00"));
	c.ready_required = false; // match_ready 0: the start waits only for players
	Match pub(c);
	pub.Begin(0);
	CHECK(pub.HudText(At(500, {P(1, false), P(2, false)})) == Hud("WARMUP", "1:00"));
}

TEST_CASE("the HUD: no timelimit counts up; overtime and sudden death")
{
	MatchConfig c = Enabled();
	c.timelimit_ms = 0;
	Match up(c);
	up.Begin(0);
	CHECK(up.HudText(At(500, {P(1, true), P(2, false)})) == Hud("READY UP 1/2", "0:00")); // it will count up
	up = Match(c);
	const int64_t live = ToLive(up);
	CHECK(up.HudText(At(live + 75000, {P(1, true), P(2, true)})) == Hud("", "1:15"));

	MatchConfig o = Enabled();
	o.overtime = 2;
	Match timed(o);
	const int64_t live2 = ToLive(timed);
	timed.Update(At(live2 + 60000, {P(1, true, 3), P(2, true, 3)}));
	REQUIRE(timed.State() == MatchState::Overtime);
	CHECK(timed.HudText(At(live2 + 62000, {P(1, true, 3), P(2, true, 3)})) == Hud("OVERTIME", "1:58"));

	o.overtime = -1;
	Match sudden(o);
	const int64_t live3 = ToLive(sudden);
	sudden.Update(At(live3 + 60000, {P(1, true, 3), P(2, true, 3)}));
	REQUIRE(sudden.State() == MatchState::Overtime);
	CHECK(sudden.HudText(At(live3 + 61000, {P(1, true, 3), P(2, true, 3)})) == Hud("SUDDEN DEATH", "0:00"));
}

TEST_CASE("the HUD in rounds: the clock, round over, the countdown, the clock")
{
	Match m(Hoony()); // pause 1 s, countdown 2 s, timelimit 60 s
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)}));
	CHECK(m.HudText(At(live + 100, {P(1, true), P(2, true)})) == Hud("", "1:00"));
	m.Update(At(live + 5000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundOver);
	CHECK(m.HudText(At(live + 5000, {})) == Hud("ROUND OVER", "0:55")); // the clock runs on
	m.Update(At(live + 6000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	CHECK(m.HudText(At(live + 6000, {})) == Hud("COUNTDOWN", "0:54"));
	CHECK(m.CountdownSeconds(live + 6000) == 2);
	CHECK(m.CountdownSeconds(live + 7500) == 1);
	m.Update(At(live + 8000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(m.HudText(At(live + 8000, {})) == Hud("", "0:52"));
	CHECK(m.CountdownSeconds(live + 8000) == 0);
}

TEST_CASE("the HUD is empty with the match flow off (stock HUD)")
{
	Match m;
	m.Begin(0);
	CHECK(m.HudText(At(1000, {P(1, true)})) == Hud("", ""));
}

TEST_CASE("overtime and sudden death sound the klaxon; the HUD names them, no centre print")
{
	auto kinds = [](const std::vector<MatchAction> &out, MatchActionKind kind)
	{ return std::count_if(out.begin(), out.end(), [&](const MatchAction &a) { return a.kind == kind; }); };
	MatchConfig c = Enabled();
	c.overtime = 2;
	Match timed(c);
	const int64_t live = ToLive(timed);
	auto out = timed.Update(At(live + 60000, {P(1, true, 3), P(2, true, 3)}));
	CHECK(kinds(out, MatchActionKind::Klaxon) == 1);
	CHECK(kinds(out, MatchActionKind::Announce) == 0);
	out = timed.Update(At(live + 60000 + 120000, {P(1, true, 4), P(2, true, 4)})); // still tied
	CHECK(kinds(out, MatchActionKind::Klaxon) == 1);

	c.overtime = -1;
	Match sudden(c);
	const int64_t live2 = ToLive(sudden);
	out = sudden.Update(At(live2 + 60000, {P(1, true, 3), P(2, true, 3)}));
	CHECK(kinds(out, MatchActionKind::Klaxon) == 1);
	CHECK(kinds(out, MatchActionKind::Announce) == 0);
}

TEST_CASE("no firing in the match countdown; the round countdown freezes outright")
{
	Match m(Enabled());
	m.Begin(0);
	CHECK_FALSE(m.HoldFire()); // warmup is free play
	m.Update(At(1000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Countdown);
	CHECK(m.HoldFire());
	CHECK_FALSE(m.Frozen()); // they can still move
	m.Update(At(4000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK_FALSE(m.HoldFire());

	Match stock; // match flow off: stock
	stock.Begin(0);
	CHECK_FALSE(stock.HoldFire());
}

// --- Stock sounds, as MuffMode plays them ------------------------------------

namespace
{
std::vector<std::string> Sounds(const std::vector<MatchAction> &out)
{
	std::vector<std::string> sounds;
	for (const MatchAction &a : out)
		if (a.kind == MatchActionKind::Sound)
			sounds.push_back(a.text);
	return sounds;
}
} // namespace

TEST_CASE("sounds: the countdown is spoken, FIGHT plays tele_up")
{
	MatchConfig c = Enabled();
	c.countdown_ms = 12000;
	Match m(c);
	m.Begin(0);
	std::vector<std::string> heard;
	for (int64_t t = 1000; t < 13000; t += 100)
		for (const std::string &s : Sounds(m.Update(At(t, {P(1, true), P(2, true)}))))
			heard.push_back(s);
	// 12 and 11 have no stock voice; 10 down to 1 do.
	CHECK(heard == std::vector<std::string>{"world/10.wav", "world/9.wav", "world/8.wav", "world/7.wav", "world/6.wav",
	                                        "world/5.wav", "world/4.wav", "world/3.wav", "world/2.wav", "world/1.wav"});
	auto out = m.Update(At(13000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(Sounds(out) == std::vector<std::string>{"misc/tele_up.wav"});
}

TEST_CASE("sounds: the last 30 and 20 seconds, then every second from 10")
{
	Match m(Enabled()); // timelimit 60 s
	const int64_t live = ToLive(m);
	std::vector<std::string> heard;
	for (int64_t t = live + 100; t < live + 60000; t += 100)
		for (const std::string &s : Sounds(m.Update(At(t, {P(1, true, 1), P(2, true, 0)}))))
			heard.push_back(s);
	REQUIRE(heard.size() == 12);
	CHECK(heard[0] == "world/30sec.wav");
	CHECK(heard[1] == "world/20sec.wav");
	CHECK(heard[2] == "world/10.wav");
	CHECK(heard[11] == "world/1.wav");
}

TEST_CASE("sounds: a won round plays flagcap; the next round's FIGHT plays tele_up")
{
	Match m(Hoony());
	const int64_t live = ToLive(m);
	m.Update(At(live + 100, {P(1, true, 0), P(2, true, 0)})); // baseline
	auto out = m.Update(At(live + 5000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundOver);
	CHECK(Sounds(out) == std::vector<std::string>{"ctf/flagcap.wav"});
	m.Update(At(live + 6000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	out = m.Update(At(live + 6100, {P(1, true, 1), P(2, true, 0)}));
	CHECK(Sounds(out) == std::vector<std::string>{"world/2.wav"});
	out = m.Update(At(live + 8000, {P(1, true, 1), P(2, true, 0)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(Sounds(out) == std::vector<std::string>{"misc/tele_up.wav"});
}

TEST_CASE("sounds: the match end plays the jingle over the scoreboard")
{
	Match m(Enabled());
	const int64_t live = ToLive(m);
	auto out = m.Update(At(live + 60000, {P(1, true, 2), P(2, true, 1)}));
	REQUIRE(m.State() == MatchState::Intermission);
	CHECK(Sounds(out) == std::vector<std::string>{"world/xian1.wav"});
}

TEST_CASE("sounds: none with the match flow off (stock)")
{
	Match m;
	m.Begin(0);
	CHECK(Sounds(m.Update(At(1000, {P(1, true)}))).empty());
}

TEST_CASE("match: with the flow off the clock counts down stock's timelimit")
{
	CHECK(Core_Match_StockHudText(0, 600000).clock == "10:00");
	CHECK(Core_Match_StockHudText(1, 600000).clock == "10:00");
	CHECK(Core_Match_StockHudText(1000, 600000).clock == "9:59");
	CHECK(Core_Match_StockHudText(599001, 600000).clock == "0:01");
	CHECK(Core_Match_StockHudText(600000, 600000).clock == "0:00");
	CHECK(Core_Match_StockHudText(700000, 600000).clock == "0:00"); // the intermission
	CHECK(Core_Match_StockHudText(5000, 600000).status.empty());
}

TEST_CASE("match: with the flow off and no timelimit, no clock")
{
	CHECK(Core_Match_StockHudText(5000, 0).clock.empty());
	CHECK(Core_Match_StockHudText(5000, 0).status.empty());
}

TEST_CASE("match: a vote's new limits reach a live match without a restart")
{
	MatchConfig c = Enabled();
	c.timelimit_ms = 10 * 60000;
	Match m(c);
	const int64_t live = ToLive(m);
	CHECK(m.ClockEndMs() == live + 10 * 60000);
	m.ChangeLimits(20 * 60000, 30, live + 5000);
	CHECK(m.State() == MatchState::Live);
	CHECK(m.ClockEndMs() == live + 20 * 60000); // still counted from LIVE
	CHECK(m.Config().fraglimit == 30);
	m.Update(At(live + 6000, {P(1, true, 29), P(2, true)}));
	CHECK(m.State() == MatchState::Live);
	m.Update(At(live + 7000, {P(1, true, 30), P(2, true)}));
	CHECK(m.State() == MatchState::Intermission); // the new fraglimit ends it
}

TEST_CASE("match: a time limit already passed ends the match at once")
{
	Match m(Enabled()); // 1 minute
	const int64_t live = ToLive(m);
	m.ChangeLimits(5 * 60000, 0, live + 30000);
	m.Update(At(live + 90000, {P(1, true), P(2, true)}));
	CHECK(m.State() == MatchState::Live); // 1:30 of 5:00
	m.ChangeLimits(1 * 60000, 0, live + 90000);
	m.Update(At(live + 90050, {P(1, true), P(2, true)}));
	CHECK(m.State() == MatchState::Intermission);
}

TEST_CASE("match: before LIVE, new limits wait for it")
{
	Match m(Enabled());
	m.Begin(0);
	m.ChangeLimits(15 * 60000, 0, 500);
	CHECK(m.State() == MatchState::Warmup);
	CHECK(m.Config().timelimit_ms == 15 * 60000);
	m.Update(At(1000, {P(1, true), P(2, true)}));
	m.Update(At(4000, {P(1, true), P(2, true)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(m.ClockEndMs() == 4000 + 15 * 60000);
}

namespace
{
MatchPlayer Bot(int id)
{
	MatchPlayer p = P(id, true);
	p.bot = true;
	return p;
}
} // namespace

TEST_CASE("match_minplayers counts bots too, but a match needs a human")
{
	Match m(Enabled()); // match_minplayers 2
	m.Begin(0);
	m.Update(At(20000, {Bot(2), Bot(3)}));
	CHECK(m.State() == MatchState::Warmup); // bots alone only warm up
	CHECK(m.HudText(At(20000, {Bot(2), Bot(3)})).status == "WAITING FOR PLAYERS");
	CHECK(m.HudText(At(20000, {P(1, false)})).status == "WAITING FOR PLAYERS"); // a lone human: one short
	m.Update(At(21000, {P(1, true), Bot(2)}));
	CHECK(m.State() == MatchState::Countdown); // one human and the bot fill
}

TEST_CASE("match_allow_no_humans: bots may play a match alone")
{
	MatchConfig c = Enabled();
	c.allow_no_humans = true;
	Match m(c);
	m.Begin(0);
	m.Update(At(20000, {Bot(1), Bot(2)}));
	CHECK(m.State() == MatchState::Countdown);
	m.Update(At(23000, {Bot(1), Bot(2)}));
	REQUIRE(m.State() == MatchState::Live);
	m.Update(At(24000, {Bot(1), Bot(2)}));
	CHECK(m.State() == MatchState::Live);
}

TEST_CASE("every human gone from a match under way: back to warmup, the arena reset")
{
	Match m(Enabled());
	const int64_t live = ToLive(m);
	// One human left keeps it going, below match_minplayers too.
	m.Update(At(live + 1000, {P(1, true, 3)}));
	CHECK(m.State() == MatchState::Live);
	// None: warmup, scores zeroed by the reset, no clock.
	const auto out = m.Update(At(live + 2000, {Bot(5)}));
	CHECK(m.State() == MatchState::Warmup);
	CHECK(Has(out, MatchActionKind::ResetArena));
	CHECK_FALSE(Has(out, MatchActionKind::EndMatch));
	CHECK(m.HudText(At(live + 2000, {Bot(5)})) == Hud("WAITING FOR PLAYERS", "1:00"));
	// A human back (and the bot): a fresh start, the full timelimit from the new LIVE.
	m.Update(At(live + 3000, {P(1, true), Bot(5)}));
	REQUIRE(m.State() == MatchState::Countdown);
	m.Update(At(live + 6000, {P(1, true), Bot(5)}));
	REQUIRE(m.State() == MatchState::Live);
	CHECK(m.HudText(At(live + 6000, {P(1, true), P(2, true)})).clock == "1:00");
}

TEST_CASE("every human gone between rounds or in overtime: back to warmup too")
{
	MatchConfig c = Enabled();
	c.overtime = -1; // sudden death
	Match m(c);
	const int64_t live = ToLive(m);
	m.Update(At(live + 60000, {P(1, true, 2), P(2, true, 2)})); // tied at the timelimit
	REQUIRE(m.State() == MatchState::Overtime);
	m.Update(At(live + 61000, {}));
	CHECK(m.State() == MatchState::Warmup);
}

// --- Elimination rounds (Clan Arena; docs/design/clan-arena.md) ---------------

namespace
{
MatchPlayer E(int id, int team, int health = 100)
{
	MatchPlayer p = T(id, team);
	p.key = "k" + std::to_string(id);
	p.health = health;
	return p;
}

std::vector<MatchPlayer> TwoVTwo()
{
	return {E(1, 1), E(2, 1), E(3, 2), E(4, 2)};
}

MatchConfig CA()
{
	MatchConfig c = TeamConfig();
	c.rounds = RoundsMode::Elimination;
	c.round_freeze = false;
	c.round_pause_ms = 1000;
	c.round_countdown_ms = 2000;
	c.timelimit_ms = 0;
	c.fraglimit = 3;
	return c;
}

// Warmup to LIVE with everyone ready; returns when round 1 began.
int64_t CaLive(Match &m, const std::vector<MatchPlayer> &players)
{
	m.Begin(0);
	m.Update(At(1000, players));
	m.Update(At(4000, players));
	REQUIRE(m.State() == MatchState::Live);
	return 4000;
}

int Won(const std::vector<MatchAction> &out)
{
	for (const MatchAction &a : out)
		if (a.kind == MatchActionKind::RoundWon)
			return a.value;
	return -99;
}

bool Announced(const std::vector<MatchAction> &out, const std::string &text)
{
	return std::any_of(out.begin(), out.end(),
	                   [&](const MatchAction &a) { return a.kind == MatchActionKind::Announce && a.text == text; });
}
} // namespace

TEST_CASE("match_rounds: 0, 1 / frag, elimination, any case; anything else is off")
{
	CHECK(Core_Match_ParseRounds("0") == RoundsMode::Off);
	CHECK(Core_Match_ParseRounds("") == RoundsMode::Off);
	CHECK(Core_Match_ParseRounds("1") == RoundsMode::Frag);
	CHECK(Core_Match_ParseRounds("Frag") == RoundsMode::Frag);
	CHECK(Core_Match_ParseRounds("ELIMINATION") == RoundsMode::Elimination);
	CHECK(Core_Match_ParseRounds("2") == RoundsMode::Off);
}

TEST_CASE("elimination: a team with no lives left loses the round; the survivors play on")
{
	Match m(CA());
	const int64_t live = CaLive(m, TwoVTwo());
	CHECK(m.HudText(At(live, TwoVTwo())).status.empty()); // no round line in play (Chris, 2026-10-30)
	CHECK(m.SideLives(At(live, TwoVTwo())).at(1) == 2);
	CHECK(m.Died(1));
	CHECK(m.Eliminated(1));
	CHECK_FALSE(m.Eliminated(2));
	CHECK(m.SideLives(At(live + 100, TwoVTwo())).at(1) == 1);
	CHECK(m.Update(At(live + 100, TwoVTwo())).empty()); // RED still has one
	CHECK(m.Died(2));
	const auto out = m.Update(At(live + 200, TwoVTwo()));
	CHECK(Won(out) == 2);
	CHECK(m.State() == MatchState::RoundOver);
	CHECK(Has(out, MatchActionKind::RoundHold));
	CHECK(m.RoundWins(2) == 1);
	CHECK(m.RoundWins(1) == 0);
}

TEST_CASE("elimination: both teams out at once is a draw, at once (MMREX stalls)")
{
	Match m(CA());
	const int64_t live = CaLive(m, TwoVTwo());
	for (int id = 1; id <= 4; ++id)
		m.Died(id);
	const auto out = m.Update(At(live + 100, TwoVTwo()));
	CHECK(Won(out) == -1);
	CHECK(m.State() == MatchState::RoundOver);
	CHECK(m.RoundWins(1) + m.RoundWins(2) == 0);
}

TEST_CASE("elimination: the next round respawns everyone, counts down without firing, then ROUND 2")
{
	Match m(CA());
	const int64_t live = CaLive(m, TwoVTwo());
	m.Died(1);
	m.Died(2);
	m.Update(At(live + 100, TwoVTwo()));
	const auto pause = m.Update(At(live + 1200, TwoVTwo()));
	CHECK(Has(pause, MatchActionKind::RoundRespawn));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	CHECK(m.HoldFire()); // match_round_freeze 0: move, no fire
	CHECK_FALSE(m.Frozen());
	CHECK(m.CountdownSeconds(live + 1200) == 2);
	const auto start = m.Update(At(live + 3300, TwoVTwo()));
	CHECK(m.State() == MatchState::Live);
	CHECK(m.Round() == 2);
	CHECK(Announced(start, "ROUND 2: FIGHT!"));
	CHECK_FALSE(m.Eliminated(1)); // a new roster, everyone in
	CHECK(m.SideLives(At(live + 3300, TwoVTwo())).at(1) == 2);
}

TEST_CASE("elimination: the round limit ends the match on the winning round, at once")
{
	MatchConfig c = CA();
	c.fraglimit = 1;
	Match m(c);
	const int64_t live = CaLive(m, TwoVTwo());
	m.Died(3);
	m.Died(4);
	const auto out = m.Update(At(live + 100, TwoVTwo()));
	CHECK(Won(out) == 1);
	CHECK(m.State() == MatchState::Intermission);
	CHECK(std::any_of(out.begin(), out.end(),
	                  [](const MatchAction &a) { return a.kind == MatchActionKind::EndMatch && a.text == "rounds"; }));
}

TEST_CASE("elimination: the round timer gives it to more lives left, then more health, else a draw")
{
	MatchConfig c = CA();
	c.round_time_ms = 60000;
	{
		Match m(c);
		const int64_t live = CaLive(m, TwoVTwo());
		CHECK(m.ClockEndMs() == live + 60000);
		CHECK(m.HudText(At(live + 15000, TwoVTwo())).clock == "0:45");
		m.Died(1);
		CHECK(m.Update(At(live + 59000, TwoVTwo())).empty());
		CHECK(Won(m.Update(At(live + 60000, TwoVTwo()))) == 2); // BLUE: 2 lives v 1
	}
	{
		Match m(c);
		const int64_t live = CaLive(m, TwoVTwo());
		const std::vector<MatchPlayer> hurt = {E(1, 1, 300), E(2, 1, 300), E(3, 2, 50), E(4, 2, 300)};
		CHECK(Won(m.Update(At(live + 60000, hurt))) == 1); // equal lives: RED has more health
	}
	{
		Match m(c);
		const int64_t live = CaLive(m, TwoVTwo());
		CHECK(Won(m.Update(At(live + 60000, TwoVTwo()))) == -1); // nothing between them
	}
}

TEST_CASE("elimination: more lives per round; a death with lives left isn't elimination")
{
	MatchConfig c = CA();
	c.round_lives = 3;
	Match m(c);
	const int64_t live = CaLive(m, TwoVTwo());
	CHECK(m.SideLives(At(live, TwoVTwo())).at(1) == 6);
	CHECK_FALSE(m.Died(1));
	CHECK_FALSE(m.Died(1));
	CHECK(m.InRound(1, "k1", 1)); // lives left: back in
	CHECK(m.Died(1));
	CHECK_FALSE(m.InRound(1, "k1", 1));
}

TEST_CASE("elimination: only the round's roster plays it; joiners, switchers and slot reuse wait")
{
	Match m(CA());
	const int64_t live = CaLive(m, TwoVTwo());
	CHECK(m.InRound(2, "k2", 1));
	CHECK_FALSE(m.InRound(5, "k5", 2));    // joined mid-round
	CHECK_FALSE(m.InRound(2, "k2", 2));    // switched team
	CHECK_FALSE(m.InRound(2, "other", 1)); // a new client in a leaver's slot
	m.Forget(2);
	CHECK_FALSE(m.InRound(2, "k2", 1));
	// Players who left don't count as alive: RED is down to player 1.
	const std::vector<MatchPlayer> one_red = {E(1, 1), E(3, 2), E(4, 2)};
	CHECK(m.SideLives(At(live + 100, one_red)).at(1) == 1);
	m.Died(1);
	CHECK(Won(m.Update(At(live + 200, one_red))) == 2);
	// Between rounds everyone may spawn; the next round's roster has the joiner.
	CHECK(m.InRound(5, "k5", 2));
}

TEST_CASE("elimination: a team with nobody on it gets no free rounds: the countdown waits")
{
	Match m(CA());
	const int64_t live = CaLive(m, TwoVTwo());
	m.Died(3);
	m.Died(4);
	m.Update(At(live + 100, TwoVTwo()));
	const std::vector<MatchPlayer> no_blue = {E(1, 1), E(2, 1)};
	m.Update(At(live + 1200, no_blue)); // the pause ends
	REQUIRE(m.State() == MatchState::RoundCountdown);
	CHECK(m.Update(At(live + 1300, no_blue)).empty());
	CHECK(m.CountdownSeconds(live + 1300) == 0);
	CHECK(m.HudText(At(live + 1300, no_blue)).status == "WAITING FOR TEAMS");
	m.Update(At(live + 9000, no_blue));
	CHECK(m.State() == MatchState::RoundCountdown); // still waiting, not a free round
	m.Update(At(live + 9100, TwoVTwo()));           // BLUE is back: a full countdown from here
	CHECK(m.CountdownSeconds(live + 9100) == 2);
	m.Update(At(live + 11200, TwoVTwo()));
	CHECK(m.State() == MatchState::Live);
	CHECK(m.Round() == 2);
}

TEST_CASE("elimination with match_round_freeze: round 1 gets a frozen countdown after the match's")
{
	MatchConfig c = CA();
	c.round_freeze = true;
	Match m(c);
	m.Begin(0);
	m.Update(At(1000, TwoVTwo()));
	const auto go = m.Update(At(4000, TwoVTwo()));
	CHECK(Has(go, MatchActionKind::ResetArena));
	CHECK_FALSE(Announced(go, "ROUND 1: FIGHT!"));
	REQUIRE(m.State() == MatchState::RoundCountdown);
	CHECK(m.Frozen());
	CHECK_FALSE(m.HoldFire());
	CHECK(m.InRound(9, "k9", 1)); // nobody held out before the round starts
	const auto start = m.Update(At(6100, TwoVTwo()));
	CHECK(m.State() == MatchState::Live);
	CHECK(m.Round() == 1);
	CHECK(Announced(start, "ROUND 1: FIGHT!"));
}

TEST_CASE("elimination without freeze: the match countdown is round 1's")
{
	Match m(CA());
	m.Begin(0);
	m.Update(At(1000, TwoVTwo()));
	const auto go = m.Update(At(4000, TwoVTwo()));
	CHECK(m.State() == MatchState::Live);
	CHECK(Announced(go, "ROUND 1: FIGHT!"));
}

TEST_CASE("elimination: the timelimit waits for the round; a tie goes to sudden death rounds")
{
	MatchConfig c = CA();
	c.timelimit_ms = 60000;
	c.overtime = -1;
	Match m(c);
	const int64_t live = CaLive(m, TwoVTwo());
	CHECK(m.Update(At(live + 70000, TwoVTwo())).empty()); // past the limit: the round is played out
	CHECK(m.State() == MatchState::Live);
	for (int id = 1; id <= 4; ++id)
		m.Died(id);
	const auto draw = m.Update(At(live + 71000, TwoVTwo())); // 0-0 at the end of a round
	CHECK(Has(draw, MatchActionKind::Klaxon));
	CHECK(m.State() == MatchState::RoundOver);
	m.Update(At(live + 72100, TwoVTwo()));
	m.Update(At(live + 74200, TwoVTwo()));
	REQUIRE(m.State() == MatchState::Overtime);
	CHECK(m.HudText(At(live + 74200, TwoVTwo())).status == "SUDDEN DEATH");
	m.Died(1);
	m.Died(2);
	const auto out = m.Update(At(live + 75000, TwoVTwo()));
	CHECK(m.State() == MatchState::Intermission);
	CHECK(std::any_of(out.begin(), out.end(), [](const MatchAction &a)
	                  { return a.kind == MatchActionKind::EndMatch && a.text == "sudden death"; }));
}

TEST_CASE("elimination without teams (Duel): a player out of lives loses the round")
{
	MatchConfig c = CA();
	c.teams = false;
	c.round_lives = 3;
	Match m(c);
	const std::vector<MatchPlayer> duel = {E(1, 0), E(2, 0)};
	const int64_t live = CaLive(m, duel);
	CHECK(m.HudText(At(live, duel)).status.empty());
	m.Died(2);
	m.Died(2);
	CHECK(m.Update(At(live + 100, duel)).empty());
	m.Died(2);
	CHECK(Won(m.Update(At(live + 200, duel))) == 1);
	CHECK(m.RoundWins(1) == 1);
}

TEST_CASE("elimination: every human gone sends it back to warmup and clears the rounds")
{
	MatchConfig c = CA();
	Match m(c);
	const int64_t live = CaLive(m, TwoVTwo());
	m.Died(3);
	m.Died(4);
	m.Update(At(live + 100, TwoVTwo()));
	REQUIRE(m.RoundWins(1) == 1);
	MatchPlayer bot = E(9, 1);
	bot.bot = true;
	m.Update(At(live + 200, {bot}));
	CHECK(m.State() == MatchState::Warmup);
	CHECK(m.RoundWins(1) == 0);
}
