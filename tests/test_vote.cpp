#include "core/vote.h"

#include "doctest.h"

namespace
{
VoteConfig AllKinds()
{
	VoteConfig c;
	c.allowed = {VoteKind::Map, VoteKind::Gametype, VoteKind::Ruleset, VoteKind::Restart};
	return c;
}

VoteChoices Choices()
{
	VoteChoices c;
	c.maps = Core_Vote_MapList("", false);
	c.gametypes = {"ffa", "duel", "tdm", "ctf"};
	c.rulesets = {"stock", "match", "hoony"};
	return c;
}

VoteProposal Map(const char *name)
{
	return {VoteKind::Map, name};
}
} // namespace

TEST_CASE("vote: kinds parse case-insensitively and describe themselves")
{
	CHECK(Core_Vote_ParseKind("MAP") == VoteKind::Map);
	CHECK(Core_Vote_ParseKind("restart") == VoteKind::Restart);
	CHECK_FALSE(Core_Vote_ParseKind("kick").has_value());
	CHECK(Core_Vote_Describe(Map("q2dm3")) == "map q2dm3");
	CHECK(Core_Vote_Describe({VoteKind::Restart, ""}) == "restart");
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
	CHECK(v.HudText() == "VOTE: MAP Q2DM3 (1/1)");
	CHECK(v.Update(voters, 1050) == VoteEvent::Passed);
	CHECK(v.HudText() == "VOTE PASSED");
	CHECK(v.Update(voters, 3000) == VoteEvent::None);
	CHECK(v.Update(voters, 4050) == VoteEvent::Execute);
	CHECK_FALSE(v.Open());
	CHECK_FALSE(v.Passed());
	CHECK(v.HudText().empty());
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
		const VoteProposal p{VoteKind::Restart, ""};
		CHECK(v.Call(0, p, Choices(), false, voters, 0) == VoteError::NotAllowed);
		CHECK(Core_Vote_ErrorText(VoteError::NotAllowed, config, p, Choices()) ==
		      "Votes on restart are off. Allowed: map.");
	}
	SUBCASE("a live match, unless midgame")
	{
		CHECK(Vote(config).Call(0, Map("q2dm2"), Choices(), true, voters, 0) == VoteError::MatchLive);
		config.midgame = true;
		CHECK(Vote(config).Call(0, Map("q2dm2"), Choices(), true, voters, 0) == VoteError::None);
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
		CHECK(v.Call(0, {VoteKind::Gametype, "tag"}, Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(v.Call(0, {VoteKind::Ruleset, "../x"}, Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(v.Call(0, {VoteKind::Restart, "now"}, Choices(), false, voters, 0) == VoteError::BadArg);
		CHECK(Core_Vote_ErrorText(VoteError::BadArg, config, Map("q2dm9"), Choices()) ==
		      "Maps: q2dm1 q2dm2 q2dm3 q2dm4 q2dm5 q2dm6 q2dm7 q2dm8.");
		CHECK(v.Call(0, {VoteKind::Gametype, "CTF"}, Choices(), false, voters, 0) == VoteError::None);
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
