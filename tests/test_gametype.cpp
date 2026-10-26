#include "doctest.h"

#include "core/gametype.h"

#include <set>
#include <string>

TEST_CASE("the registry holds ffa, tag and ball with unique lowercase ids")
{
	std::set<std::string> ids;
	for (size_t i = 0; i < core_gametype_count; ++i)
	{
		const std::string id = core_gametypes[i]->id;
		CHECK(ids.insert(id).second);
		for (char c : id)
			CHECK(((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'));
		CHECK(std::string(core_gametypes[i]->title).size() > 0);
	}
	CHECK(ids.count("ffa") == 1);
	CHECK(ids.count("tag") == 1);
	CHECK(ids.count("ball") == 1);
}

TEST_CASE("ffa is stock deathmatch: no Rogue rules, no hooks")
{
	const GametypeDef *ffa = Core_Gametype_Find(core_gametypes, core_gametype_count, "ffa");
	REQUIRE(ffa);
	CHECK(ffa->rogue_gamerules == 0);
	CHECK(ffa->install == nullptr);
}

TEST_CASE("the stock gamerules values select Rogue's modes")
{
	const GametypeDef *tag = Core_Gametype_ForRogueGamerules(core_gametypes, core_gametype_count, 2);
	const GametypeDef *ball = Core_Gametype_ForRogueGamerules(core_gametypes, core_gametype_count, 3);
	REQUIRE(tag);
	REQUIRE(ball);
	CHECK(std::string(tag->id) == "tag");
	CHECK(std::string(ball->id) == "ball");
	CHECK(Core_Gametype_ForRogueGamerules(core_gametypes, core_gametype_count, 0) == nullptr);
	CHECK(Core_Gametype_ForRogueGamerules(core_gametypes, core_gametype_count, 7) == nullptr);
}

TEST_CASE("lookup is exact and works on any table")
{
	static const GametypeDef a = {"alpha", "Alpha", 0, nullptr};
	static const GametypeDef b = {"beta", "Beta", 5, nullptr};
	const GametypeDef *const table[] = {&a, &b};
	CHECK(Core_Gametype_Find(table, 2, "beta") == &b);
	CHECK(Core_Gametype_Find(table, 2, "bet") == nullptr);
	CHECK(Core_Gametype_Find(table, 2, "") == nullptr);
	CHECK(Core_Gametype_ForRogueGamerules(table, 2, 5) == &b);
}
