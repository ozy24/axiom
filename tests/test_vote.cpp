#include "core/vote.h"

#include "doctest.h"

namespace
{
VoteConfig AllKinds()
{
	VoteConfig c;
	c.allowed = {VoteKind::Map, VoteKind::Gametype, VoteKind::Restart};
	return c;
}

VoteChoices Choices()
{
	VoteChoices c;
	c.maps = Core_Vote_MapList("", false);
	c.gametypes = {"stock", "match", "duel", "hoony", "tdm", "ctf"};
	return c;
}

VoteProposal Map(const char *name)
{
	return {VoteKind::Map, name, ""};
}
} // namespace

TEST_CASE("vote: kinds parse case-insensitively and describe themselves")
{
	CHECK(Core_Vote_ParseKind("MAP") == VoteKind::Map);
	CHECK(Core_Vote_ParseKind("restart") == VoteKind::Restart);
	CHECK(Core_Vote_ParseKind("ruleset") == VoteKind::Gametype); // the older word
	CHECK_FALSE(Core_Vote_ParseKind("kick").has_value());
	CHECK(Core_Vote_Describe(Map("q2dm3")) == "map q2dm3");
	CHECK(Core_Vote_Describe({VoteKind::Restart, "", ""}) == "restart");
	CHECK(Core_Vote_Describe({VoteKind::Gametype, "hoony", "Duel: HoonyMode"}) == "gametype Duel: HoonyMode");
}

TEST_CASE("vote: list words are lowercased, deduplicated, and unsafe ones dropped")
{
	CHECK(Core_Vote_Words("  Q2DM1 q2dm2\tq2dm1 bad;map \"x\" ../up ok_map-2 ") ==
	      std::vector<std::string>{"q2dm1", "q2dm2", "ok_map-2"});
	CHECK(Core_Vote_Words(std::string(33, 'a')).empty());
}

TEST_CASE("vote: the map list defaults to the stock maps for the family")
{
	CHECK(Core_Vote_MapList("", false).size() == 8);
	CHECK(Core_Vote_MapList("", false).front() == "q2dm1");
	CHECK(Core_Vote_MapList("", true) == std::vector<std::string>{"q2ctf1", "q2ctf2", "q2ctf3", "q2ctf4", "q2ctf5"});
	CHECK(Core_Vote_MapList("mm-mcoil q2dm1", true) == std::vector<std::string>{"mm-mcoil", "q2dm1"});
}

TEST_CASE("vote: a gametype change keeps the map if the new list has it")
{
	const auto ctf = Core_Vote_MapList("", true);
	CHECK(Core_Vote_MapFor(ctf, "q2dm1") == "q2ctf1");
	CHECK(Core_Vote_MapFor(ctf, "Q2CTF3") == "q2ctf3");
	CHECK(Core_Vote_MapFor({}, "q2dm1") == "q2dm1");
}

TEST_CASE("vote: a lone voter's call passes, then runs after the delay")
{
	Vote v(AllKinds());
	const std::vector<int> voters{0};
	REQUIRE(v.Call(0, Map("Q2DM3"), Choices(), false, voters, 1000) == VoteError::None);
	CHECK(v.Open());
	CHECK(v.Proposal() == Map("q2dm3"));
	CHECK(v.HudText(1000) == "VOTE: MAP Q2DM3 - YES 1/1 - 30s");
	CHECK(v.Update(voters, 1050) == VoteEvent::Passed);
	CHECK(v.HudText(1050) == "VOTE PASSED");
	CHECK(v.Update(voters, 3000) == VoteEvent::None);
	CHECK(v.Update(voters, 4050) == VoteEvent::Execute);
	CHECK_FALSE(v.Open());
	CHECK_FALSE(v.Passed());
	CHECK(v.HudText(4050).empty());
}

TEST_CASE("vote: a majority of the voters there now decides")
{
	Vote v(AllKinds());
	const std::vector<int> four{0, 1, 2, 3};
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, four, 0) == VoteError::None);
	CHECK(v.Update(four, 10) == VoteEvent::None); // 1 of 4
	CHECK(v.Cast(1, true, four) == VoteError::None);
	CHECK(v.Update(four, 20) == VoteEvent::None); // 2 of 4 is not a majority
	CHECK(v.Tally().yes == 2);
	CHECK(v.Cast(2, true, four) == VoteError::None);
	CHECK(v.Update(four, 30) == VoteEvent::Passed);
}

TEST_CASE("vote: it fails as soon as it can't pass")
{
	Vote v(AllKinds());
	const std::vector<int> four{0, 1, 2, 3};
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, four, 0) == VoteError::None);
	CHECK(v.Cast(1, false, four) == VoteError::None);
	CHECK(v.Update(four, 10) == VoteEvent::None);
	CHECK(v.Cast(2, false, four) == VoteError::None); // 2 against of 4: yes can reach 2 at most
	CHECK(v.Update(four, 20) == VoteEvent::Failed);
	CHECK_FALSE(v.Open());
}

TEST_CASE("vote: answers can change; the same answer again is refused")
{
	Vote v(AllKinds());
	const std::vector<int> three{0, 1, 2};
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, three, 0) == VoteError::None);
	CHECK(v.Cast(0, true, three) == VoteError::AlreadyVoted);
	CHECK(v.Cast(1, false, three) == VoteError::None);
	CHECK(v.Answer(1) == false);
	CHECK(v.Cast(1, true, three) == VoteError::None);
	CHECK(v.Answer(1) == true);
	CHECK_FALSE(v.Answer(2).has_value());
	CHECK(v.Update(three, 10) == VoteEvent::Passed);
}

TEST_CASE("vote: leavers drop out of the count, and new voters join it")
{
	Vote v(AllKinds());
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, {0, 1, 2}, 0) == VoteError::None);
	CHECK(v.Update({0, 1, 2}, 10) == VoteEvent::None);
	CHECK(v.Cast(1, true, {0, 1, 2}) == VoteError::None);
	CHECK(v.Update({0, 2, 3, 4}, 20) == VoteEvent::None); // 1 had voted yes, then left: 1 of 4
	CHECK(v.Tally().voters == 4);
	CHECK(v.Tally().yes == 1);
	CHECK(v.Update({}, 30) == VoteEvent::Failed); // everyone left
}

TEST_CASE("vote: no majority in time: it times out and the caller waits")
{
	Vote v(AllKinds());
	const std::vector<int> three{0, 1, 2};
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, three, 0) == VoteError::None);
	CHECK(v.SecondsLeft(0) == 30);
	CHECK(v.SecondsLeft(29001) == 1);
	CHECK(v.Update(three, 29999) == VoteEvent::None);
	CHECK(v.Update(three, 30000) == VoteEvent::TimedOut);
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, three, 40000) == VoteError::Cooldown);
	CHECK(v.Call(1, Map("q2dm2"), Choices(), false, three, 40000) == VoteError::None); // others needn't
}

TEST_CASE("vote: the cooldown ends")
{
	Vote v(AllKinds());
	const std::vector<int> two{0, 1};
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, two, 0) == VoteError::None);
	CHECK(v.Cast(1, false, two) == VoteError::None);
	CHECK(v.Update(two, 10) == VoteEvent::Failed);
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, two, 30009) == VoteError::Cooldown);
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, two, 30010) == VoteError::None);
}

TEST_CASE("vote: calls are refused for the right reasons")
{
	VoteConfig config = AllKinds();
	const std::vector<int> voters{0, 1};

	SUBCASE("voting off")
	{
		config.enabled = false;
		CHECK(Vote(config).Call(0, Map("q2dm2"), Choices(), false, voters, 0) == VoteError::Disabled);
	}
	SUBCASE("kind not allowed")
	{
		config.allowed = {VoteKind::Map};
		Vote v(config);
		const VoteProposal p{VoteKind::Restart, "", ""};
		CHECK(v.Call(0, p, Choices(), false, voters, 0) == VoteError::NotAllowed);
		CHECK(Core_Vote_ErrorText(VoteError::NotAllowed, config, p, Choices()) ==
		      "Votes on restart are off. Allowed: map.");
	}
	SUBCASE("a live match: allowed by default (a wrong setting can be voted away), off with midgame 0")
	{
		CHECK(Vote(config).Call(0, Map("q2dm2"), Choices(), true, voters, 0) == VoteError::None);
		config.midgame = false;
		CHECK(Vote(config).Call(0, Map("q2dm2"), Choices(), true, voters, 0) == VoteError::MatchLive);
	}
	SUBCASE("one at a time")
	{
		Vote v(config);
		REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, voters, 0) == VoteError::None);
		CHECK(v.Call(1, Map("q2dm3"), Choices(), false, voters, 0) == VoteError::InProgress);
	}
	SUBCASE("spectators and bots")
	{
		Vote v(config);
		CHECK(v.Call(5, Map("q2dm2"), Choices(), false, voters, 0) == VoteError::NotVoter);
		REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, voters, 0) == VoteError::None);
		CHECK(v.Cast(5, true, voters) == VoteError::NotVoter);
	}
	SUBCASE("arguments must be on the lists")
	{
		Vote v(config);
		CHECK(v.Call(0, Map("q2dm9"), Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(v.Call(0, {VoteKind::Gametype, "tag", ""}, Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(v.Call(0, {VoteKind::Gametype, "../x", ""}, Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(v.Call(0, {VoteKind::Restart, "now", ""}, Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(Core_Vote_ErrorText(VoteError::BadArg, config, Map("q2dm9"), Choices()) ==
		      "Maps: q2dm1 q2dm2 q2dm3 q2dm4 q2dm5 q2dm6 q2dm7 q2dm8.");
		CHECK(v.Call(0, {VoteKind::Gametype, "CTF", ""}, Choices(), false, voters, 0) == VoteError::None);
	}
	SUBCASE("no vote to answer")
	{
		CHECK(Vote(config).Cast(0, true, voters) == VoteError::NoVote);
	}
}

TEST_CASE("vote: each player calls a limited number per map; a new map resets it")
{
	VoteConfig config = AllKinds();
	config.call_limit = 2;
	Vote v(config);
	const std::vector<int> two{0, 1};
	auto call_and_pass = [&](int64_t at)
	{
		REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, two, at) == VoteError::None);
		REQUIRE(v.Cast(1, true, two) == VoteError::None);
		REQUIRE(v.Update(two, at + 1) == VoteEvent::Passed);
		REQUIRE(v.Update(two, at + 5000) == VoteEvent::Execute);
	};
	call_and_pass(0);
	call_and_pass(10000);
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, two, 20000) == VoteError::LimitReached);
	v.Reset();
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, two, 20000) == VoteError::None);
}

TEST_CASE("vote: a gametype shows on the HUD by its name")
{
	Vote v(AllKinds());
	REQUIRE(v.Call(0, {VoteKind::Gametype, "hoony", "Duel: HoonyMode"}, Choices(), false, {0, 1}, 0) ==
	        VoteError::None);
	CHECK(v.HudText(2500) == "VOTE: DUEL: HOONYMODE - YES 1/2 - 28s");
}

TEST_CASE("vote: the server's call has no answer of its own and skips the player limits")
{
	VoteConfig off; // nothing allowed, voting off: only the admin can call
	off.enabled = false;
	off.midgame = false;
	Vote v(off);
	const std::vector<int> voters{0, 1};
	REQUIRE(v.Call(Vote::kServer, Map("q2dm2"), Choices(), true, voters, 0) == VoteError::None);
	CHECK(v.HudText(0) == "VOTE: MAP Q2DM2 - YES 0/2 - 30s");
	CHECK(v.Call(Vote::kServer, Map("q2dm3"), Choices(), true, voters, 0) == VoteError::InProgress);
	REQUIRE(v.Cast(0, false, voters) == VoteError::None);
	CHECK(v.Update(voters, 10) == VoteEvent::Failed);
	// No cooldown for the server; the lists still apply.
	CHECK(v.Call(Vote::kServer, Map("nowhere"), Choices(), true, voters, 20) == VoteError::BadArg);
	REQUIRE(v.Call(Vote::kServer, Map("q2dm3"), Choices(), true, voters, 20) == VoteError::None);
	REQUIRE(v.Cast(0, true, voters) == VoteError::None);
	REQUIRE(v.Cast(1, true, voters) == VoteError::None);
	CHECK(v.Update(voters, 30) == VoteEvent::Passed);
}

TEST_CASE("vote: a slot's next client doesn't inherit its calls or cooldown")
{
	VoteConfig c = AllKinds();
	c.call_limit = 1;
	Vote v(c);
	const std::vector<int> voters{0, 1};
	REQUIRE(v.Call(0, Map("q2dm2"), Choices(), false, voters, 0) == VoteError::None);
	REQUIRE(v.Cast(1, false, voters) == VoteError::None);
	REQUIRE(v.Update(voters, 10) == VoteEvent::Failed);
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, voters, 20) == VoteError::LimitReached);
	v.Forget(0); // they left; someone else takes slot 0
	CHECK(v.Call(0, Map("q2dm2"), Choices(), false, voters, 30) == VoteError::None);
}

namespace
{
VoteConfig Settings()
{
	VoteConfig c;
	c.allowed = {VoteKind::Timelimit, VoteKind::Scorelimit, VoteKind::Powerups, VoteKind::FriendlyFire};
	return c;
}

VoteChoices Now(int timelimit, int scorelimit, bool teams = false, bool ctf = false)
{
	VoteChoices c = Choices();
	c.now.timelimit_min = timelimit;
	c.now.scorelimit = scorelimit;
	c.now.teams = teams;
	c.now.ctf = ctf;
	return c;
}

VoteError CallSetting(VoteKind kind, const char *arg, const VoteChoices &choices, Vote *out = nullptr)
{
	Vote local(Settings());
	Vote &v = out ? *out : local;
	return v.Call(0, {kind, arg, ""}, choices, false, {0, 1, 2}, 0);
}
} // namespace

TEST_CASE("vote settings: names and aliases")
{
	CHECK(Core_Vote_ParseKind("TimeLimit") == VoteKind::Timelimit);
	CHECK(Core_Vote_ParseKind("fraglimit") == VoteKind::Scorelimit);
	CHECK(Core_Vote_ParseKind("capturelimit") == VoteKind::Scorelimit);
	CHECK(Core_Vote_ParseKind("powerups") == VoteKind::Powerups);
	CHECK(Core_Vote_ParseKind("friendlyfire") == VoteKind::FriendlyFire);
	for (VoteKind k : Core_Vote_AllKinds())
		CHECK(Core_Vote_ParseKind(VoteKindName(k)) == k);
	CHECK_FALSE(Core_Vote_Setting(VoteKind::Map, false).has_value());
	CHECK(Core_Vote_Setting(VoteKind::Scorelimit, false)->title == std::string("Frag Limit"));
	CHECK(Core_Vote_Setting(VoteKind::Scorelimit, true)->title == std::string("Capture Limit"));
}

TEST_CASE("vote settings: values must be in range")
{
	const VoteSetting time = *Core_Vote_Setting(VoteKind::Timelimit, false);
	CHECK(Core_Vote_SettingValue(time, "20") == 20);
	CHECK(Core_Vote_SettingValue(time, "60") == 60);
	for (const char *bad : {"0", "61", "-5", "10.5", "", "ten", "1e3", "9999999"})
		CHECK_MESSAGE(!Core_Vote_SettingValue(time, bad).has_value(), bad);
	const VoteSetting frags = *Core_Vote_Setting(VoteKind::Scorelimit, false);
	CHECK(Core_Vote_SettingValue(frags, "0") == 0);
	CHECK(Core_Vote_SettingValue(frags, "100") == 100);
	CHECK_FALSE(Core_Vote_SettingValue(frags, "101").has_value());
	const VoteSetting caps = *Core_Vote_Setting(VoteKind::Scorelimit, true);
	CHECK_FALSE(Core_Vote_SettingValue(caps, "21").has_value());
	const VoteSetting pu = *Core_Vote_Setting(VoteKind::Powerups, false);
	CHECK(Core_Vote_SettingValue(pu, "OFF") == 0);
	CHECK(Core_Vote_SettingValue(pu, "1") == 1);
	CHECK(Core_Vote_SettingValue(pu, "yes") == 1);
	CHECK_FALSE(Core_Vote_SettingValue(pu, "2").has_value());
}

TEST_CASE("vote settings: every menu choice is in range")
{
	for (bool ctf : {false, true})
		for (VoteKind k : Core_Vote_AllKinds())
			if (const std::optional<VoteSetting> s = Core_Vote_Setting(k, ctf))
				for (int value : s->menu)
					CHECK(Core_Vote_SettingValue(*s, Core_Vote_SettingArg(*s, value)) == value);
}

TEST_CASE("vote settings: labels say what changes")
{
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::Timelimit, false), 20) == "time limit 20 minutes");
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::Timelimit, false), 1) == "time limit 1 minute");
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::Scorelimit, false), 30) == "frag limit 30");
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::Scorelimit, false), 0) == "no frag limit");
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::Scorelimit, true), 5) == "capture limit 5");
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::Powerups, false), 0) == "powerups off");
	CHECK(Core_Vote_SettingLabel(*Core_Vote_Setting(VoteKind::FriendlyFire, false), 1) == "friendly fire on");
}

TEST_CASE("vote settings: a call normalises its argument and shows its label")
{
	Vote v(Settings());
	REQUIRE(CallSetting(VoteKind::Powerups, "0", Now(10, 0), &v) == VoteError::None);
	CHECK(v.Proposal().arg == "off");
	CHECK(Core_Vote_Describe(v.Proposal()) == "powerups off");
	CHECK(v.HudText(0) == "VOTE: POWERUPS OFF - YES 1/3 - 30s");

	Vote t(Settings());
	REQUIRE(CallSetting(VoteKind::Timelimit, "020", Now(10, 0), &t) == VoteError::None);
	CHECK(t.Proposal().arg == "20");
	CHECK(t.HudText(0) == "VOTE: TIME LIMIT 20 MINUTES - YES 1/3 - 30s");
}

TEST_CASE("vote settings: refused out of range, unchanged, or outside their modes")
{
	CHECK(CallSetting(VoteKind::Timelimit, "90", Now(10, 0)) == VoteError::BadArg);
	CHECK(CallSetting(VoteKind::Timelimit, "10", Now(10, 0)) == VoteError::AlreadySet);
	CHECK(CallSetting(VoteKind::Scorelimit, "20", Now(10, 20)) == VoteError::AlreadySet);
	CHECK(CallSetting(VoteKind::Powerups, "on", Now(10, 0)) == VoteError::AlreadySet); // on by default
	CHECK(CallSetting(VoteKind::FriendlyFire, "on", Now(10, 0)) == VoteError::TeamsOnly);
	CHECK(CallSetting(VoteKind::FriendlyFire, "on", Now(10, 0, true)) == VoteError::None);
	CHECK(CallSetting(VoteKind::FriendlyFire, "off", Now(10, 0, true)) == VoteError::AlreadySet);
}

TEST_CASE("vote settings: no score limit needs a time limit")
{
	CHECK(CallSetting(VoteKind::Scorelimit, "0", Now(0, 20)) == VoteError::NoLimit);
	CHECK(CallSetting(VoteKind::Scorelimit, "0", Now(10, 20)) == VoteError::None);
}

TEST_CASE("vote settings: errors explain themselves")
{
	const VoteConfig c = Settings();
	const VoteChoices ch = Now(10, 0, false, true);
	CHECK(Core_Vote_ErrorText(VoteError::BadArg, c, {VoteKind::Timelimit, "90", ""}, ch) ==
	      "Usage: callvote timelimit 1-60 (minutes)");
	CHECK(Core_Vote_ErrorText(VoteError::BadArg, c, {VoteKind::Scorelimit, "x", ""}, ch) ==
	      "Usage: callvote scorelimit 0-20 (0: none)"); // CTF's capture range
	CHECK(Core_Vote_ErrorText(VoteError::BadArg, c, {VoteKind::Powerups, "x", ""}, ch) ==
	      "Usage: callvote powerups on|off");
	CHECK(Core_Vote_ErrorText(VoteError::AlreadySet, c, {VoteKind::Timelimit, "10", ""}, ch) ==
	      "Already set: time limit 10 minutes.");
}

TEST_CASE("vote settings: not allowed unless listed")
{
	Vote v(AllKinds()); // map gametype restart
	CHECK(v.Call(0, {VoteKind::Timelimit, "20", ""}, Now(10, 0), false, {0, 1}, 0) == VoteError::NotAllowed);
}

namespace
{
VoteConfig ReadyVotes()
{
	VoteConfig c;
	c.allowed = {VoteKind::ReadyAll, VoteKind::ReadyTeam};
	return c;
}

VoteChoices ReadyUp(bool teams, bool ready_up = true)
{
	VoteChoices c = Choices();
	c.now.teams = teams;
	c.now.ready_up = ready_up;
	return c;
}
} // namespace

TEST_CASE("vote ready: readyall and readyteam name and describe themselves")
{
	CHECK(Core_Vote_ParseKind("ReadyAll") == VoteKind::ReadyAll);
	CHECK(Core_Vote_ParseKind("readyteam") == VoteKind::ReadyTeam);
	CHECK_FALSE(Core_Vote_Setting(VoteKind::ReadyAll, false).has_value());
	Vote v(ReadyVotes());
	REQUIRE(v.Call(0, {VoteKind::ReadyAll, "", ""}, ReadyUp(false), false, {0, 1, 2}, 0) == VoteError::None);
	CHECK(Core_Vote_Describe(v.Proposal()) == "ready everyone");
	CHECK(v.HudText(0) == "VOTE: READY EVERYONE - YES 1/3 - 30s");
	Vote t(ReadyVotes());
	REQUIRE(t.Call(0, {VoteKind::ReadyTeam, "Red", ""}, ReadyUp(true), false, {0, 1}, 0) == VoteError::None);
	CHECK(t.Proposal().arg == "red");
	CHECK(t.HudText(0) == "VOTE: READY RED - YES 1/2 - 30s");
}

TEST_CASE("vote ready: only in a ready-up; readyteam only in team modes, for a team")
{
	Vote v(ReadyVotes());
	CHECK(v.Call(0, {VoteKind::ReadyAll, "", ""}, ReadyUp(false, false), false, {0, 1}, 0) == VoteError::NoReadyUp);
	CHECK(v.Call(0, {VoteKind::ReadyAll, "now", ""}, ReadyUp(false), false, {0, 1}, 0) == VoteError::BadArg);
	CHECK(v.Call(0, {VoteKind::ReadyTeam, "red", ""}, ReadyUp(false), false, {0, 1}, 0) == VoteError::TeamsOnly);
	CHECK(v.Call(0, {VoteKind::ReadyTeam, "red", ""}, ReadyUp(true, false), false, {0, 1}, 0) == VoteError::NoReadyUp);
	CHECK(v.Call(0, {VoteKind::ReadyTeam, "", ""}, ReadyUp(true), false, {0, 1}, 0) == VoteError::BadArg);
	const VoteConfig c = ReadyVotes();
	CHECK(Core_Vote_ErrorText(VoteError::BadArg, c, {VoteKind::ReadyTeam, "", ""}, ReadyUp(true)) ==
	      "Join a team first.");
	CHECK(Core_Vote_ErrorText(VoteError::TeamsOnly, c, {VoteKind::ReadyTeam, "red", ""}, ReadyUp(false)) ==
	      "Readyteam is only for team gametypes.");
	CHECK(Core_Vote_ErrorText(VoteError::NotVoter, c, {VoteKind::ReadyTeam, "red", ""}, ReadyUp(true)) ==
	      "Only RED votes on readying RED.");
}

TEST_CASE("vote ready: readyteam's voters are the team the adapter passes")
{
	// The adapter passes only the caller's team as voters: two of them, so
	// one other yes passes it whatever the other team thinks.
	Vote v(ReadyVotes());
	REQUIRE(v.Call(0, {VoteKind::ReadyTeam, "blue", ""}, ReadyUp(true), false, {0, 3}, 0) == VoteError::None);
	CHECK(v.Cast(1, false, {0, 3}) == VoteError::NotVoter);
	CHECK(v.Cast(3, true, {0, 3}) == VoteError::None);
	CHECK(v.Update({0, 3}, 0) == VoteEvent::Passed);
}

namespace
{
VoteConfig ShuffleVotes()
{
	VoteConfig c;
	c.allowed = {VoteKind::Shuffle};
	return c;
}

VoteChoices TeamWarmup(int players, bool warmup = true)
{
	VoteChoices c = Choices();
	c.now.teams = true;
	c.now.warmup = warmup;
	c.now.team_players = players;
	return c;
}
} // namespace

TEST_CASE("vote shuffle: names and describes itself")
{
	CHECK(Core_Vote_ParseKind("Shuffle") == VoteKind::Shuffle);
	CHECK_FALSE(Core_Vote_Setting(VoteKind::Shuffle, false).has_value());
	Vote v(ShuffleVotes());
	REQUIRE(v.Call(0, {VoteKind::Shuffle, "", ""}, TeamWarmup(4), false, {0, 1, 2, 3}, 0) == VoteError::None);
	CHECK(Core_Vote_Describe(v.Proposal()) == "shuffle teams");
	CHECK(v.HudText(0) == "VOTE: SHUFFLE TEAMS - YES 1/4 - 30s");
}

TEST_CASE("vote shuffle: team modes' warmup only, with three players on teams")
{
	Vote v(ShuffleVotes());
	VoteChoices ffa = TeamWarmup(4);
	ffa.now.teams = false;
	CHECK(v.Call(0, {VoteKind::Shuffle, "", ""}, ffa, false, {0, 1}, 0) == VoteError::TeamsOnly);
	CHECK(v.Call(0, {VoteKind::Shuffle, "", ""}, TeamWarmup(4, false), true, {0, 1}, 0) == VoteError::WarmupOnly);
	CHECK(v.Call(0, {VoteKind::Shuffle, "", ""}, TeamWarmup(2), false, {0, 1}, 0) == VoteError::TooFew);
	CHECK(v.Call(0, {VoteKind::Shuffle, "now", ""}, TeamWarmup(4), false, {0, 1}, 0) == VoteError::BadArg);
	CHECK_FALSE(v.Open());
	const VoteConfig c = ShuffleVotes();
	const VoteProposal p{VoteKind::Shuffle, "", ""};
	CHECK(Core_Vote_ErrorText(VoteError::TeamsOnly, c, p, ffa) == "Shuffle is only for team gametypes.");
	CHECK(Core_Vote_ErrorText(VoteError::WarmupOnly, c, p, ffa) == "Teams can only be shuffled in warmup.");
	CHECK(Core_Vote_ErrorText(VoteError::TooFew, c, p, ffa) == "A shuffle needs at least three players on teams.");
	CHECK(Core_Vote_ErrorText(VoteError::BadArg, c, p, ffa) == "Usage: callvote shuffle");
}

TEST_CASE("vote shuffle: the admin's sv callvote still needs the warmup")
{
	Vote v(ShuffleVotes());
	CHECK(v.Call(Vote::kServer, {VoteKind::Shuffle, "", ""}, TeamWarmup(4, false), true, {0, 1}, 0) ==
	      VoteError::WarmupOnly);
	CHECK(v.Call(Vote::kServer, {VoteKind::Shuffle, "", ""}, TeamWarmup(4), false, {0, 1}, 0) == VoteError::None);
}

TEST_CASE("vote settings: in elimination rounds the score limit is a Round Limit, 1-20")
{
	const VoteSetting s = *Core_Vote_Setting(VoteKind::Scorelimit, false, true);
	CHECK(s.title == std::string("Round Limit"));
	CHECK(s.min == 1);
	CHECK(s.max == 20);
	CHECK(Core_Vote_SettingLabel(s, 8) == "round limit 8");
	CHECK_FALSE(Core_Vote_SettingValue(s, "0").has_value()); // a round match always has a limit
}
