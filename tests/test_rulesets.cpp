#include "doctest.h"

#include "core/rulesets.h"

#include <set>
#include <sstream>
#include <string>

// Every cvar a built-in ruleset may set. A typo, or a mod-prefixed name
// (decision 0004), fails here instead of silently doing nothing.
static const std::set<std::string> kKnownCvars = {
    "g_gametype",     "match_warmup", "match_ready", "match_minplayers", "match_warmup_time",    "match_countdown",
    "match_overtime", "timelimit",    "fraglimit",   "match_rounds",     "match_round_countdown"};

TEST_CASE("built-in rulesets only set known cvars, one `set` per line")
{
	REQUIRE(core_ruleset_count >= 2);
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		const RulesetDef &r = core_rulesets[i];
		CAPTURE(r.name);
		CHECK(Core_Ruleset_ValidName(r.name));
		CHECK(std::string(r.description).size() > 0);
		std::istringstream lines(r.cfg);
		std::string line;
		int count = 0;
		while (std::getline(lines, line))
		{
			std::istringstream words(line);
			std::string verb, cvar, value;
			words >> verb >> cvar >> value;
			CAPTURE(line);
			CHECK(verb == "set");
			CHECK(kKnownCvars.count(cvar) == 1);
			CHECK_FALSE(value.empty());
			++count;
		}
		CHECK(count > 0);
	}
}

TEST_CASE("duel selects the duel gametype with the match flow on")
{
	const RulesetDef *duel = Core_Ruleset_Find("duel");
	REQUIRE(duel);
	CHECK(std::string(duel->cfg).find("set g_gametype duel\n") != std::string::npos);
	CHECK(std::string(duel->cfg).find("set match_warmup 1\n") != std::string::npos);
}

TEST_CASE("every built-in is a complete profile: it sets the same cvars")
{
	auto cvars_of = [](const char *cfg)
	{
		std::set<std::string> out;
		std::istringstream lines(cfg);
		std::string line, verb, cvar;
		while (std::getline(lines, line))
		{
			std::istringstream words(line);
			words >> verb >> cvar;
			out.insert(cvar);
		}
		return out;
	};
	const std::set<std::string> first = cvars_of(core_rulesets[0].cfg);
	for (size_t i = 1; i < core_ruleset_count; ++i)
	{
		CAPTURE(core_rulesets[i].name);
		std::set<std::string> these = cvars_of(core_rulesets[i].cfg);
		// stock leaves the server's own limits alone; the others set them
		these.erase("timelimit");
		these.erase("fraglimit");
		CHECK(these == first);
	}
}

TEST_CASE("hoony is a duel with rounds on")
{
	const RulesetDef *hoony = Core_Ruleset_Find("hoony");
	REQUIRE(hoony);
	CHECK(std::string(hoony->cfg).find("set g_gametype duel\n") != std::string::npos);
	CHECK(std::string(hoony->cfg).find("set match_rounds 1\n") != std::string::npos);
}

TEST_CASE("stock puts the match flow off")
{
	const RulesetDef *stock = Core_Ruleset_Find("stock");
	REQUIRE(stock);
	CHECK(std::string(stock->cfg).find("set match_warmup 0\n") != std::string::npos);
}

TEST_CASE("names are restricted so exec can't leave rulesets/")
{
	CHECK(Core_Ruleset_ValidName("duel"));
	CHECK(Core_Ruleset_ValidName("ctf-comp_2"));
	CHECK_FALSE(Core_Ruleset_ValidName(""));
	CHECK_FALSE(Core_Ruleset_ValidName("../server"));
	CHECK_FALSE(Core_Ruleset_ValidName("a/b"));
	CHECK_FALSE(Core_Ruleset_ValidName("Duel"));
	CHECK_FALSE(Core_Ruleset_ValidName("x;quit"));
	CHECK_FALSE(Core_Ruleset_ValidName(std::string(33, 'a')));
}

TEST_CASE("commands: a built-in's cfg, else exec from rulesets/, then g_ruleset")
{
	const std::string match = Core_Ruleset_Commands("match");
	CHECK(match.find("set match_warmup 1\n") != std::string::npos);
	CHECK(match.find("exec") == std::string::npos);
	CHECK(match.substr(match.size() - std::string("set g_ruleset match\n").size()) == "set g_ruleset match\n");
	CHECK(Core_Ruleset_Commands("clan") == "exec rulesets/clan.cfg\nset g_ruleset clan\n");
	CHECK(Core_Ruleset_Commands("../x").empty());
}
