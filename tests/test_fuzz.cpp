// Hostile input for every pure function that reads text from outside the
// game: files on disk (.loc, .ent, ruleset .cfg), cvars an admin types
// (g_map_list, g_ruleset_variants) and what players send (callouts, vote
// arguments, skins, names). Each runs on thousands of seeded random inputs
// built from bytes and fragments that have broken parsers before: quotes,
// braces, comments, '%' and '$' tokens, control and high bytes, long runs.
// The checks are each function's documented bounds; the CI `sanitize` job
// runs this under AddressSanitizer and UBSan, which is where it earns its
// keep.
#include "core/entities.h"
#include "core/loc.h"
#include "core/maps.h"
#include "core/menu.h"
#include "core/miniscore.h"
#include "core/rulesets.h"
#include "core/scoreboard.h"
#include "core/vote.h"

#include "doctest.h"

#include <random>
#include <set>
#include <string>

namespace
{
constexpr int kRounds = 3000;

const char *const kFragments[] = {
    "\"",
    "\"\"",
    "{",
    "}",
    "{ \"classname\" \"worldspawn\" }",
    "//",
    "/*",
    "*/",
    "\n",
    "\r\n",
    "\t",
    " ",
    ";",
    "\\",
    "%",
    "%l",
    "%h",
    "%n",
    "%N",
    "%%",
    "%s",
    "%n%n%n",
    "{}",
    "{0}",
    "{:>999999}",
    "$",
    "$g_",
    "##P",
    "##P99",
    "*",
    "-",
    "+",
    "e",
    "1e999",
    "nan",
    "-0",
    "0x",
    "9999999999999999999",
    "set",
    "seta",
    "exec",
    "x",
    "..",
    "/",
    "ctf -40 ",
    "ctf 200 ",
    "xv 0 yv 0 string \"",
    "male/grunt",
    "q2dm1",
    "q2ctf1",
    "weapon_",
    "item_",
    "ammo_",
};

std::string Hostile(std::mt19937 &rng)
{
	std::string s;
	const int parts = std::uniform_int_distribution<int>(0, 24)(rng);
	for (int i = 0; i < parts; ++i)
	{
		switch (std::uniform_int_distribution<int>(0, 3)(rng))
		{
		case 0: // any byte, NUL included
			s += static_cast<char>(std::uniform_int_distribution<int>(0, 255)(rng));
			break;
		case 1: // a long run
			s.append(std::uniform_int_distribution<size_t>(1, 700)(rng),
			         static_cast<char>(std::uniform_int_distribution<int>(32, 126)(rng)));
			break;
		default:
			s += kFragments[std::uniform_int_distribution<size_t>(0, std::size(kFragments) - 1)(rng)];
		}
	}
	return s;
}
} // namespace

TEST_CASE("fuzz: .loc files and callouts")
{
	std::mt19937 rng(27);
	LocValues values;
	for (int i = 0; i < kRounds; ++i)
	{
		const std::string text = Hostile(rng);
		size_t skipped = 0;
		for (const LocPlace &p : Core_Loc_ParseFile(text, skipped))
		{
			REQUIRE(!p.label.empty());
			REQUIRE(p.label.size() <= kLocMaxLabel);
		}
		REQUIRE(Core_Loc_CleanLabel(text).size() <= kLocMaxLabel);
		values.place = Core_Loc_CleanLabel(Hostile(rng));
		values.team = Hostile(rng);
		REQUIRE(Core_Loc_Expand(Core_Loc_Text(text), values).size() <= kLocMaxText);
		REQUIRE(!Core_Loc_ItemName(text).empty());
	}
}

TEST_CASE("fuzz: .ent files and ruleset .cfg files")
{
	std::mt19937 rng(23);
	std::string error;
	for (int i = 0; i < kRounds; ++i)
	{
		const std::string text = Hostile(rng);
		(void)Core_Entities_ValidFile(text, error);
		for (const RulesetCfgStep &step : Core_Ruleset_ParseCfg(text))
			REQUIRE(!step.name.empty());
		(void)Core_Ruleset_ValidName(text);
		(void)Core_Ruleset_All(text, {"ffa", "duel", "tdm", "ctf"});
	}
}

TEST_CASE("fuzz: map lists and vote arguments")
{
	std::mt19937 rng(14);
	for (int i = 0; i < kRounds; ++i)
	{
		const std::string text = Hostile(rng);
		for (const std::string &word : Core_Vote_Words(text))
			REQUIRE(!word.empty());
		(void)Core_Maps_Words(text);
		const std::vector<std::string> list = Core_Vote_MapList(text, i % 2);
		(void)Core_Vote_MapFor(list, Hostile(rng));
		(void)Core_Vote_ParseKind(text);
		for (const VoteKind kind : Core_Vote_AllKinds())
		{
			if (const auto setting = Core_Vote_Setting(kind, i % 2))
			{
				if (const auto value = Core_Vote_SettingValue(*setting, text))
				{
					(void)Core_Vote_SettingText(*setting, *value);
					(void)Core_Vote_SettingArg(*setting, *value);
				}
			}
		}
	}
}

TEST_CASE("fuzz: names, skins and layouts from players")
{
	std::mt19937 rng(18);
	for (int i = 0; i < kRounds; ++i)
	{
		const std::string text = Hostile(rng);
		const std::string clean = Core_Menu_Clean(text);
		REQUIRE(clean.find('"') == std::string::npos);
		const std::string icon = Core_Miniscore_IconPath(text);
		REQUIRE(icon.find("..") == std::string::npos);
		const std::string ready = Core_Scoreboard_TeamReady(text, {0, 1, 7, 255}, 1400);
		REQUIRE(text.size() + ready.size() <= std::max<size_t>(text.size(), 1400));
	}
}
