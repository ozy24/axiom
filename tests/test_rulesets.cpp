#include "doctest.h"

#include "core/rulesets.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>

// Every cvar a built-in may set is a gameplay cvar, which a switch resets
// (decision 0013). A typo, a mod-prefixed name (decision 0004), or a setting
// that would leak into the next mode fails here.
static std::set<std::string> GameplayCvars()
{
	std::set<std::string> out;
	for (size_t i = 0; i < core_gameplay_cvar_count; ++i)
		out.insert(core_gameplay_cvars[i].name);
	return out;
}
static const std::set<std::string> kKnownCvars = GameplayCvars();

static const std::vector<std::string> kGametypes = {"ffa", "duel", "tdm", "ctf"};

static size_t Find(const std::string &text, const std::string &what)
{
	return text.find(what);
}

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

TEST_CASE("each gametype has one built-in of its own, named after it, that selects it")
{
	for (const std::string &gt : kGametypes)
	{
		CAPTURE(gt);
		int own = 0;
		for (size_t i = 0; i < core_ruleset_count; ++i)
			own += !core_rulesets[i].variant && gt == core_rulesets[i].gametype;
		CHECK(own == 1);
		const RulesetDef *def = Core_Ruleset_Find(gt);
		REQUIRE(def);
		CHECK_FALSE(def->variant);
		CHECK(Find(def->cfg, "set g_gametype " + gt + "\n") != std::string::npos);
		CHECK(Find(def->cfg, "set match_flow 1\n") != std::string::npos);
	}
}

TEST_CASE("every built-in sets ctf and teamplay to match its gametype")
{
	// Core_Gametype_PreInit lets `ctf 1` / `teamplay 1` pick CTF / TDM when
	// g_gametype is ffa (stock configs), so a gametype's own settings must
	// never leave one on.
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (core_rulesets[i].variant)
			continue;
		const std::string cfg = core_rulesets[i].cfg;
		const std::string gt = core_rulesets[i].gametype;
		CAPTURE(gt);
		CHECK(Find(cfg, gt == "ctf" ? "set ctf 1\n" : "set ctf 0\n") != std::string::npos);
		CHECK(Find(cfg, gt == "tdm" ? "set teamplay 1\n" : "set teamplay 0\n") != std::string::npos);
	}
}

TEST_CASE("every gametype's own built-in sets every match_* cvar")
{
	// A ruleset switch resets these anyway; spelling them out keeps each
	// built-in readable as the whole of its match settings.
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (core_rulesets[i].variant)
			continue;
		const std::string cfg = core_rulesets[i].cfg;
		CAPTURE(core_rulesets[i].name);
		for (size_t c = 0; c < core_gameplay_cvar_count; ++c)
		{
			const std::string name = core_gameplay_cvars[c].name;
			if (name.rfind("match_", 0) != 0)
				continue;
			CAPTURE(name);
			CHECK(Find(cfg, "set " + name + " ") != std::string::npos);
		}
	}
}

TEST_CASE("team games lock their teams as the match goes live; the others have none to lock")
{
	// Competitive by default (Chris, 2026-10-29); the compiled default stays stock.
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (core_rulesets[i].variant)
			continue;
		const std::string gt = core_rulesets[i].gametype;
		CAPTURE(gt);
		const bool teams = gt == "tdm" || gt == "ctf";
		CHECK(Find(core_rulesets[i].cfg, teams ? "set match_lock 1\n" : "set match_lock 0\n") != std::string::npos);
	}
}

TEST_CASE("a built-in variant states only its differences, never its gametype")
{
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		CAPTURE(core_rulesets[i].name);
		if (core_rulesets[i].variant)
			CHECK(Find(core_rulesets[i].cfg, "g_gametype") == std::string::npos);
	}
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

TEST_CASE("resolve: built-ins, and an admin's variants listed in g_ruleset_variants")
{
	// No built-in variants (decision 0022); an admin can list their own.
	CHECK_FALSE(Core_Ruleset_Resolve("hoony", "", kGametypes));
	const auto hoony = Core_Ruleset_Resolve("duel_hoony", "duel_hoony", kGametypes);
	REQUIRE(hoony);
	CHECK(hoony->gametype == "duel");
	CHECK(hoony->variant == "Hoony");
	CHECK(hoony->builtin == nullptr);

	const auto insta = Core_Ruleset_Resolve("ctf_instagib", "ffa_rocket_arena, ctf_instagib", kGametypes);
	REQUIRE(insta);
	CHECK(insta->gametype == "ctf");
	CHECK(insta->variant == "Instagib");
	CHECK(insta->builtin == nullptr);
	CHECK(Core_Ruleset_Resolve("ffa_rocket_arena", "ffa_rocket_arena", kGametypes)->variant == "Rocket Arena");

	CHECK_FALSE(Core_Ruleset_Resolve("ctf_instagib", "", kGametypes));           // not listed
	CHECK_FALSE(Core_Ruleset_Resolve("instagib", "instagib", kGametypes));       // no gametype in its name
	CHECK_FALSE(Core_Ruleset_Resolve("tag_x", "tag_x", kGametypes));             // not a gametype
	CHECK_FALSE(Core_Ruleset_Resolve("ctf_", "ctf_", kGametypes));               // no variant
	CHECK_FALSE(Core_Ruleset_Resolve("../ctf_x", "../ctf_x", kGametypes));       // unsafe
	CHECK(Core_Ruleset_Resolve("duel", "duel", kGametypes)->builtin != nullptr); // a built-in stays one
}

TEST_CASE("all: by gametype, its own first, then built-in variants, then the admin's")
{
	std::vector<std::string> names;
	for (const Ruleset &r : Core_Ruleset_All("ctf_instagib duel_x duel_x junk", kGametypes))
		names.push_back(r.name);
	CHECK(names == std::vector<std::string>{"ffa", "duel", "duel_x", "tdm", "ctf", "ctf_instagib"});
}

TEST_CASE("a switch resets every gameplay cvar, then the gametype, then the variant")
{
	const std::string text = Core_Ruleset_Commands(*Core_Ruleset_Resolve("duel_x", "duel_x", kGametypes));
	// 1. every gameplay cvar to its default, before anything else
	for (size_t i = 0; i < core_gameplay_cvar_count; ++i)
	{
		const std::string reset =
		    std::string("set ") + core_gameplay_cvars[i].name + " \"" + core_gameplay_cvars[i].value + "\"\n";
		CAPTURE(reset);
		CHECK(Find(text, reset) < Find(text, "set g_gametype duel\n"));
	}
	// 2. the gametype's own settings, then the admin's cfg for it
	CHECK(Find(text, "set g_gametype duel\n") < Find(text, "exec rulesets/duel.cfg\n"));
	// 3. the variant's file
	CHECK(Find(text, "exec rulesets/duel.cfg\n") < Find(text, "exec rulesets/duel_x.cfg\n"));
	const std::string tail = "set g_ruleset duel_x\n";
	CHECK(Find(text, "g_ruleset_applied") == std::string::npos); // write-protected: the adapter sets it
	CHECK(text.substr(text.size() - tail.size()) == tail);
}

TEST_CASE("a gametype's own switch has no variant step; an admin's variant execs its file")
{
	const std::string tdm = Core_Ruleset_Commands(*Core_Ruleset_Resolve("tdm", "", kGametypes));
	CHECK(Find(tdm, "exec rulesets/tdm.cfg\n") != std::string::npos);
	CHECK(Find(tdm, "exec") == Find(tdm, "exec rulesets/tdm.cfg\n")); // the only exec
	CHECK(Find(tdm, "set match_overtime ") != std::string::npos);     // a variant's settings don't follow

	const std::string insta = Core_Ruleset_Commands(*Core_Ruleset_Resolve("ctf_instagib", "ctf_instagib", kGametypes));
	CHECK(Find(insta, "set g_gametype ctf\n") < Find(insta, "exec rulesets/ctf.cfg\n"));
	CHECK(Find(insta, "exec rulesets/ctf.cfg\n") < Find(insta, "exec rulesets/ctf_instagib.cfg\n"));
}

TEST_CASE("the settings set at load: the reset, then the gametype's and the variant's own, in order")
{
	const auto settings = Core_Ruleset_ParseCfg(Core_Ruleset_Commands(*Core_Ruleset_Resolve("duel", "", kGametypes)));
	auto last = [&](const std::string &cvar)
	{
		std::string value = "(unset)";
		for (const RulesetCfgStep &s : settings)
			if (!s.exec && s.name == cvar)
				value = s.value;
		return value;
	};
	CHECK(settings.front().name == core_gameplay_cvars[0].name); // the reset comes first
	CHECK(last("g_gametype") == "duel");
	CHECK(last("match_flow") == "1"); // the duel's own
	CHECK(last("timelimit") == "10");
	CHECK(last("g_map_list") == ""); // reset, quotes stripped

	// An admin's variant: the gametype's own settings, then its file's exec.
	const auto insta =
	    Core_Ruleset_ParseCfg(Core_Ruleset_Commands(*Core_Ruleset_Resolve("ctf_instagib", "ctf_instagib", kGametypes)));
	CHECK(std::any_of(insta.begin(), insta.end(),
	                  [](const RulesetCfgStep &s) { return s.name == "ctf" && s.value == "1"; }));
	CHECK(std::any_of(insta.begin(), insta.end(),
	                  [](const RulesetCfgStep &s) { return s.exec && s.name == "rulesets/ctf_instagib.cfg"; }));
}

TEST_CASE("ruleset files: their set and exec lines, split as the console splits them")
{
	const std::vector<RulesetCfgStep> steps = Core_Ruleset_ParseCfg("// Bloody Blastard's FFA\n"
	                                                                "set fraglimit 30\n"
	                                                                "seta timelimit \"15\"   // was 20\r\n"
	                                                                "SET g_map_list \"q2dm1 q2dm3; q2dm8\"\n"
	                                                                "set g_no_items 1 // a; b\n"
	                                                                "fraglimit 40\n"
	                                                                "exec rulesets/more.cfg; set g_auto_join 1\n"
	                                                                "set lonely\n"
	                                                                "  sets hostname \"BB\" s");
	REQUIRE(steps.size() == 7);
	CHECK((!steps[0].exec && steps[0].name == "fraglimit" && steps[0].value == "30"));
	CHECK((steps[1].name == "timelimit" && steps[1].value == "15"));
	CHECK((steps[2].name == "g_map_list" && steps[2].value == "q2dm1 q2dm3; q2dm8")); // a quoted ; stays
	CHECK((steps[3].name == "g_no_items" && steps[3].value == "1"));
	// The console splits on the ; inside that comment: " b" is a line of its own, and nothing.
	CHECK((steps[4].exec && steps[4].name == "rulesets/more.cfg"));
	CHECK((steps[5].name == "g_auto_join" && steps[5].value == "1"));
	CHECK((steps[6].name == "hostname" && steps[6].value == "BB"));
	CHECK(Core_Ruleset_ParseCfg("").empty());
}

TEST_CASE("ruleset files: a switch's own text reads back as its sets and its exec")
{
	const std::optional<Ruleset> r = Core_Ruleset_Resolve("tdm", "", {"ffa", "duel", "tdm", "ctf"});
	REQUIRE(r);
	const std::vector<RulesetCfgStep> steps = Core_Ruleset_ParseCfg(Core_Ruleset_Commands(*r));
	const auto exec = std::find_if(steps.begin(), steps.end(), [](const RulesetCfgStep &s) { return s.exec; });
	REQUIRE(exec != steps.end());
	CHECK(exec->name == "rulesets/tdm.cfg");
	CHECK(std::any_of(steps.begin(), exec,
	                  [](const RulesetCfgStep &s) { return s.name == "teamplay" && s.value == "1"; }));
	CHECK((steps.back().name == "g_ruleset" && steps.back().value == "tdm"));
}
