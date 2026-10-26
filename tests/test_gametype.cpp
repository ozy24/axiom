#include "doctest.h"

#include "core/gametype.h"

#include <set>
#include <string>

TEST_CASE("the registry holds ffa, with unique lowercase ids")
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
}

TEST_CASE("ffa is stock deathmatch: no hooks")
{
	const GametypeDef *ffa = Core_Gametype_Find(core_gametypes, core_gametype_count, "ffa");
	REQUIRE(ffa);
	CHECK(ffa->install == nullptr);
}

TEST_CASE("Rogue's stock modes are not registry gametypes")
{
	CHECK(Core_Gametype_Find(core_gametypes, core_gametype_count, "tag") == nullptr);
	CHECK(Core_Gametype_Find(core_gametypes, core_gametype_count, "ball") == nullptr);
}

TEST_CASE("lookup is exact and works on any table")
{
	static const GametypeDef a = {"alpha", "Alpha", nullptr};
	static const GametypeDef b = {"beta", "Beta", nullptr};
	const GametypeDef *const table[] = {&a, &b};
	CHECK(Core_Gametype_Find(table, 2, "beta") == &b);
	CHECK(Core_Gametype_Find(table, 2, "bet") == nullptr);
	CHECK(Core_Gametype_Find(table, 2, "") == nullptr);
}
