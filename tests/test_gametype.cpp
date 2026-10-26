// The registry's contents are checked at run time by harness tier 4 (every
// gametype boots); its gametypes' hooks need the engine, so the generated
// registry isn't linked here. These cover lookup and ffa's definition.
#include "doctest.h"

#include "core/gametype.h"

#include <string>

TEST_CASE("ffa is stock deathmatch: no hooks")
{
	extern const GametypeDef gametype_ffa;
	const GametypeDef *ffa = &gametype_ffa;
	CHECK(std::string(ffa->id) == "ffa");
	CHECK(std::string(ffa->retail_mode) == "deathmatch");
	CHECK(ffa->install == nullptr);
	CHECK(ffa->frame == nullptr);
	CHECK(ffa->match_over == nullptr);
	CHECK(ffa->match_ended == nullptr);
	CHECK(ffa->client_command == nullptr);
}

TEST_CASE("lookup is exact and works on any table")
{
	static const GametypeDef a = {"alpha", "Alpha", "deathmatch", nullptr, nullptr, nullptr, nullptr, nullptr};
	static const GametypeDef b = {"beta", "Beta", "deathmatch", nullptr, nullptr, nullptr, nullptr, nullptr};
	const GametypeDef *const table[] = {&a, &b};
	CHECK(Core_Gametype_Find(table, 2, "beta") == &b);
	CHECK(Core_Gametype_Find(table, 2, "bet") == nullptr);
	CHECK(Core_Gametype_Find(table, 2, "") == nullptr);
}
