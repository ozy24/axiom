#include "doctest.h"

#include "core/host_files.h"
#include "core/match_record.h"

#include <json/json.h>
#include <sstream>

namespace
{
Json::Value Parse(const std::string &text)
{
	Json::Value v;
	std::string errors;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(Json::CharReaderBuilder(), in, &v, &errors));
	return v;
}

MatchRecord Sample()
{
	MatchRecord r;
	r.mod_name = "Name";
	r.mod_id = "name";
	r.mod_version = "0.1.0";
	r.mod_build = "abc";
	r.gametype = "duel";
	r.ruleset = "duel";
	r.map = "q2dm1";
	r.ended_at_utc = "2026-10-26T19:30:05Z";
	r.reason = "timelimit";
	r.live_ms = 600500;
	r.players = {{"Spec", 0, false, false}, {"Bob", 3, true, true}, {"Ann", 7, true, false}};
	return r;
}
} // namespace

TEST_CASE("the record carries the mod, the match and the players, best first")
{
	const Json::Value v = Parse(Core_MatchRecord_ToJson(Sample()));
	CHECK(v["schema"].asString() == "q2game-match/0");
	CHECK(v["mod"]["version"].asString() == "0.1.0");
	CHECK(v["gametype"].asString() == "duel");
	CHECK(v["map"].asString() == "q2dm1");
	CHECK(v["reason"].asString() == "timelimit");
	CHECK(v["live_seconds"].asInt64() == 600);
	REQUIRE(v["players"].size() == 3);
	CHECK(v["players"][0]["name"].asString() == "Ann");
	CHECK(v["players"][1]["name"].asString() == "Bob");
	CHECK(v["players"][1]["bot"].asBool());
	CHECK(v["players"][2]["name"].asString() == "Spec"); // spectators last
	CHECK_FALSE(v["players"][2]["playing"].asBool());
	CHECK(v["winner"].asString() == "Ann");
}

TEST_CASE("a tie at the top has no winner")
{
	MatchRecord r = Sample();
	r.players = {{"A", 4, true, false}, {"B", 4, true, false}};
	CHECK(Parse(Core_MatchRecord_ToJson(r))["winner"].isNull());
}

TEST_CASE("names with quotes and unicode stay valid JSON")
{
	MatchRecord r = Sample();
	r.players = {{"\"Q\\uote\" \xc3\xa9", 1, true, false}, {"x", 0, true, false}};
	const Json::Value v = Parse(Core_MatchRecord_ToJson(r));
	CHECK(v["players"][0]["name"].asString() == "\"Q\\uote\" \xc3\xa9");
}

TEST_CASE("the file name is the UTC stamp and a sanitised map")
{
	CHECK(Core_MatchRecord_FileName("2026-10-26T19:30:05Z", "q2dm1") == "20261026-193005-q2dm1.json");
	CHECK(Core_MatchRecord_FileName("2026-10-26T19:30:05Z", "../../etc/x") == "20261026-193005-______etc_x.json");
	CHECK(Core_MatchRecord_FileName("2026-10-26T19:30:05Z", "") == "20261026-193005-unknown.json");
}

TEST_CASE("a match log directory becomes a game path the engine accepts")
{
	CHECK(Core_GamePath("matches", "20261026-195500-q2dm1.json") == "matches/20261026-195500-q2dm1.json");
	CHECK(Core_GamePath("logs/duel/", "a.json") == "logs/duel/a.json");
	CHECK(Core_GamePath("", "a.json").empty());
	CHECK(Core_GamePath("/srv/q2/matches", "a.json").empty()); // absolute: written directly
	CHECK(Core_GamePath("C:\\q2\\matches", "a.json").empty());
	CHECK(Core_GamePath("../matches", "a.json").empty());
	CHECK(Core_GamePath("a/./b", "a.json").empty());
	CHECK(Core_GamePath(".hidden", "a.json").empty());
	CHECK(Core_GamePath("my matches", "a.json").empty());
	CHECK(Core_GamePath(std::string(50, 'm'), "20261026-195500-q2dm1.json").empty()); // 64 bytes or more
}
