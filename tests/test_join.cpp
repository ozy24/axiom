#include "doctest.h"

#include "core/join.h"

TEST_CASE("a human new to the server arrives spectating; bots, the host and g_auto_join go in")
{
	CHECK_FALSE(Core_Join_Arrive({}, false).play);
	CHECK(Core_Join_Arrive({}, true).play);
	JoinWho bot;
	bot.bot = true;
	CHECK(Core_Join_Arrive(bot, false).play);
	JoinWho host;
	host.host = true;
	CHECK(Core_Join_Arrive(host, false).play);
	CHECK(Core_Join_Arrive(host, false).team == 0); // the smaller team
}

TEST_CASE("the last level's players come back as they were, on their team")
{
	JoinWho back;
	back.remembered = true;
	back.team = 2;
	const JoinArrival a = Core_Join_Arrive(back, false);
	CHECK(a.play);
	CHECK(a.team == 2);
}

TEST_CASE("join memory: who was playing, by key, through a cvar's text")
{
	JoinMemory m;
	m.Set({{"id:Steam-76561198000000001", 1}, {"id:EOS-abc", 0}, {"slot:3", 2}});
	const std::string text = m.Encode();
	CHECK(text.size() == 3 * 10 + 2); // "xxxxxxxx:t" each, space separated
	const JoinMemory back = JoinMemory::Decode(text);
	int team = -1;
	CHECK(back.Recall("id:Steam-76561198000000001", team));
	CHECK(team == 1);
	CHECK(back.Recall("id:EOS-abc", team));
	CHECK(team == 0);
	CHECK_FALSE(back.Recall("id:someone-else", team));
	// The same players always give the same text (written only on a change).
	JoinMemory again;
	again.Set({{"slot:3", 2}, {"id:EOS-abc", 0}, {"id:Steam-76561198000000001", 1}});
	CHECK(again.Encode() == text);
}

TEST_CASE("join memory: anything malformed in the cvar is skipped, not trusted")
{
	const std::string good = Core_Join_Hash("id:a") + ":2";
	const JoinMemory m = JoinMemory::Decode("junk  " + good + " ZZZZZZZZ:1 12345678:9 1234567:1 " + good + "x");
	int team = 0;
	CHECK(m.Recall("id:a", team));
	CHECK(team == 2);
	CHECK(JoinMemory::Decode("").Empty());
	CHECK(JoinMemory::Decode("12345678:7").Empty());
}

TEST_CASE("the key hash is stable and 8 hex digits")
{
	CHECK(Core_Join_Hash("") == "811c9dc5");
	CHECK(Core_Join_Hash("id:a").size() == 8);
	CHECK(Core_Join_Hash("id:a") != Core_Join_Hash("id:b"));
}

TEST_CASE("early in a level, the last level's players not back yet stay remembered")
{
	JoinMemory last;
	last.Set({{"id:a", 1}, {"id:b", 2}, {"id:c", 0}});
	JoinMemory now;
	now.Set({{"id:a", 1}}); // a is back and playing; b not back yet; c back but spectating
	now.KeepAbsent(last, {"id:a", "id:c"});
	int team = 0;
	CHECK(now.Recall("id:a", team));
	CHECK(now.Recall("id:b", team));
	CHECK(team == 2);
	CHECK_FALSE(now.Recall("id:c", team)); // came back and chose to watch
}
