#include "core/menu.h"

#include <algorithm>
#include "doctest.h"

namespace
{
std::vector<MenuAction> Actions(const MenuPage &page)
{
	std::vector<MenuAction> out;
	for (const MenuRow &row : page)
		if (row.action != MenuAction::None)
			out.push_back(row.action);
	return out;
}

bool Shows(const MenuPage &page, const std::string &text)
{
	return std::any_of(page.begin(), page.end(), [&](const MenuRow &r) { return r.text == text; });
}

int RowOf(const MenuPage &page, MenuAction action)
{
	for (int i = 0; i < (int)page.size(); ++i)
		if (page[i].action == action)
			return i;
	return -1;
}

MenuView Ffa()
{
	MenuView v;
	v.hostname = "Playtest";
	v.gametype = "Free For All";
	v.map = "The Edge";
	v.map_file = "q2dm1";
	v.ruleset = "match";
	v.match_flow = true;
	v.timelimit_min = 10;
	return v;
}

MenuView Teams()
{
	MenuView v = Ffa();
	v.gametype = "Capture The Flag";
	v.teams = v.ctf = true;
	return v;
}

// Every page at its longest: 28-character texts everywhere and every row
// that can show.
MenuView Longest()
{
	MenuView v = Teams();
	v.hostname = v.gametype = v.map = v.map_file = v.ruleset = std::string(60, 'W');
	v.red = v.blue = 99;
	v.team = 0;
	v.ready_up = v.ready_required = v.rounds = true;
	v.overtime = -1;
	v.capturelimit = v.fraglimit = 999;
	v.timelimit_min = 999;
	v.voter = true;
	v.vote_proposal = std::string(60, 'W');
	v.vote_kinds = {"map", "gametype", "restart"};
	for (int i = 0; i < 40; ++i)
	{
		const std::string name = std::to_string(i) + std::string(30, 'W');
		v.vote_maps.push_back(name);
		MenuVoteGametype gt{name, name, {}};
		for (int r = 0; r < (i % 2 ? 1 : 40); ++r) // single rulesets and long variant lists
			gt.rulesets.emplace_back(name + std::to_string(r), name);
		v.vote_gametypes.push_back(gt);
	}
	return v;
}

MenuView Voting(MenuView v)
{
	v.voter = true;
	v.vote_can_call = true;
	v.vote_kinds = {"map", "gametype", "restart"};
	v.vote_maps = {"q2dm1", "q2dm2", "q2dm3"};
	v.vote_gametypes = {{"ffa", "Free For All", {{"ffa", "Free For All"}, {"ffa_casual", "Casual"}}},
	                    {"duel", "Duel", {{"duel", "Duel"}, {"hoony", "HoonyMode"}}},
	                    {"tdm", "Team Deathmatch", {{"tdm", "Team Deathmatch"}}}};
	v.gametype_id = "ffa";
	return v;
}

const MenuRow *Row(const MenuPage &page, const std::string &text)
{
	for (const MenuRow &r : page)
		if (r.text == text)
			return &r;
	return nullptr;
}
} // namespace

TEST_CASE("menu text: control bytes, quotes, braces, markers and length are made safe")
{
	CHECK(Core_Menu_Clean("Bob") == "Bob");
	CHECK(Core_Menu_Clean("a\"b\\c") == "a'b/c");
	CHECK(Core_Menu_Clean("{0}{1}") == "(0)(1)");
	CHECK(Core_Menu_Clean("x\ny\x01z") == "xyz");
	CHECK(Core_Menu_Clean("*$*name") == "name"); // not a highlight marker, not a loc key
	CHECK(Core_Menu_Clean(std::string(40, 'a')) == std::string(25, 'a') + "...");
	CHECK(Core_Menu_Clean(std::string(28, 'a')).size() == 28);
}

TEST_CASE("menu layout: stock's PMenu_Do_Update format")
{
	MenuPage page = {{"Title", MenuAlign::Center, MenuAction::None, true, {}},
	                 {},
	                 {"Go", MenuAlign::Left, MenuAction::Close, false, {}},
	                 {"R", MenuAlign::Right, MenuAction::None, false, {}}};
	CHECK(Core_Menu_Layout(page, 2) == "xv 32 yv 8 picn inventory "
	                                   "yv 32 xv 0 loc_cstring2 1 \"Title\" \"\" "
	                                   "yv 48 xv 64 loc_string2 1 \"Go\" \"\" xv 56 string2 \">\" "
	                                   "yv 56 xv 260 loc_rstring 1 \"R\" \"\" ");
}

TEST_CASE("menu pages fit stock's 18 rows and the client's 1024-byte layout")
{
	MenuView v = Longest();
	for (int state = 0; state < 4; ++state)
	{
		v.playing = state & 1;
		v.vote_open = state & 2;
		v.vote_can_call = !v.vote_open;
		for (const MenuPage &page :
		     {Core_Menu_Main(v), Core_Menu_MatchInfo(v), Core_Menu_CallVote(v), Core_Menu_VoteMaps(v),
		      Core_Menu_VoteGametypes(v), Core_Menu_VoteVariants(v, v.vote_gametypes[0].id)})
		{
			CHECK(page.size() == (size_t)kMenuRows);
			for (const MenuRow &row : page)
				CHECK(row.text.size() <= kMenuLineChars);
			for (int cur = -1; cur < kMenuRows; ++cur)
				CHECK(Core_Menu_Layout(page, cur).size() < kMenuLayoutBytes);
		}
	}
}

TEST_CASE("menu cursor: skips text rows and wraps, as stock's")
{
	const MenuPage page = Core_Menu_Main(Ffa());
	const int join = RowOf(page, MenuAction::JoinGame);
	const int chase = RowOf(page, MenuAction::Chase);
	const int info = RowOf(page, MenuAction::MatchInfo);
	const int close = RowOf(page, MenuAction::Close);
	CHECK(Core_Menu_Next(page, -1) == join);
	CHECK(Core_Menu_Next(page, join) == chase);
	CHECK(Core_Menu_Next(page, chase) == info);
	CHECK(Core_Menu_Next(page, info) == close);
	CHECK(Core_Menu_Next(page, close) == join); // wraps
	CHECK(Core_Menu_Prev(page, join) == close);
	CHECK(Core_Menu_Next(MenuPage(3), -1) == -1);
}

TEST_CASE("menu: FFA spectator joins or chases; a player can spectate")
{
	MenuView v = Ffa();
	MenuPage page = Core_Menu_Main(v);
	CHECK(Actions(page) ==
	      std::vector<MenuAction>{MenuAction::JoinGame, MenuAction::Chase, MenuAction::MatchInfo, MenuAction::Close});
	CHECK(Shows(page, "Join the Match"));
	v.chasing = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::StopChase) >= 0);
	v.playing = true;
	v.chasing = false;
	CHECK(Actions(Core_Menu_Main(v)) ==
	      std::vector<MenuAction>{MenuAction::Spectate, MenuAction::MatchInfo, MenuAction::Close});
}

TEST_CASE("menu: ready-up only for a player, only while it's on")
{
	MenuView v = Ffa();
	v.playing = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::Ready) < 0);
	v.ready_up = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::Ready) >= 0);
	v.ready = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::NotReady) >= 0);
	v.playing = false;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::NotReady) < 0);
}

TEST_CASE("menu: teams list both sides; the cursor starts on the smaller team")
{
	MenuView v = Teams();
	v.red = 3;
	v.blue = 1;
	MenuPage page = Core_Menu_Main(v);
	CHECK(Shows(page, "Join Red"));
	CHECK(Shows(page, "Join Blue"));
	CHECK(RowOf(page, MenuAction::Spectate) < 0); // already not playing
	CHECK(Core_Menu_MainCursor(page, v) == RowOf(page, MenuAction::JoinBlue));
	v.red = 0;
	CHECK(Core_Menu_MainCursor(Core_Menu_Main(v), v) == RowOf(Core_Menu_Main(v), MenuAction::JoinRed));
	v.red = v.blue = 2; // even: the first choice
	CHECK(Core_Menu_MainCursor(Core_Menu_Main(v), v) == RowOf(Core_Menu_Main(v), MenuAction::JoinRed));

	// On red: red still listed, highlighted and skipped; switch to blue or spectate; no chase camera
	v.team = 1;
	v.playing = true;
	page = Core_Menu_Main(v);
	CHECK(Shows(page, "Join Red"));
	CHECK(std::any_of(page.begin(), page.end(), [](const MenuRow &r) { return r.text == "Join Red" && r.highlight; }));
	CHECK(Actions(page) == std::vector<MenuAction>{MenuAction::JoinBlue, MenuAction::Spectate, MenuAction::MatchInfo,
	                                               MenuAction::Close});
}

TEST_CASE("menu: Duel offers the queue, never join or spectate (the queue decides)")
{
	MenuView v = Ffa();
	v.duel = true;
	CHECK(Actions(Core_Menu_Main(v)) ==
	      std::vector<MenuAction>{MenuAction::Queue, MenuAction::Chase, MenuAction::MatchInfo, MenuAction::Close});
	v.playing = true;
	CHECK(Actions(Core_Menu_Main(v)) ==
	      std::vector<MenuAction>{MenuAction::Queue, MenuAction::MatchInfo, MenuAction::Close});
}

TEST_CASE("menu: match info names the rules, and returns to the main page")
{
	MenuView v = Teams();
	v.capturelimit = 8;
	v.overtime = -1;
	v.ready_required = true;
	MenuPage page = Core_Menu_MatchInfo(v);
	CHECK(Shows(page, "Capturelimit: 8"));
	CHECK(Shows(page, "Timelimit: 10 min"));
	CHECK(Shows(page, "Overtime: sudden death"));
	CHECK(Shows(page, "Ready-up: on"));
	CHECK(page.back().action == MenuAction::Back);
	CHECK(page.back().text == "$g_pc_return");

	v.match_flow = false;
	CHECK(Shows(Core_Menu_MatchInfo(v), "No warmup: stock rules"));
}

TEST_CASE("menu: an open vote leads the main page, the cursor on Vote Yes")
{
	MenuView v = Voting(Ffa());
	v.playing = true;
	v.vote_open = true;
	v.vote_can_call = false;
	v.vote_proposal = "map q2dm3";
	MenuPage page = Core_Menu_Main(v);
	CHECK(page[4].text == "Vote: Map q2dm3");
	CHECK(page[5].text.empty());
	CHECK(page[4].action == MenuAction::None);
	CHECK(page[6].action == MenuAction::VoteYes);
	CHECK(page[7].action == MenuAction::VoteNo);
	CHECK(Core_Menu_MainCursor(page, v) == 6);
	CHECK(RowOf(page, MenuAction::CallVote) < 0);

	// Answered: that row is marked and skipped; the cursor goes to the other.
	v.vote_answer = 1;
	page = Core_Menu_Main(v);
	CHECK(page[6].highlight);
	CHECK(page[6].action == MenuAction::None);
	CHECK(RowOf(page, MenuAction::VoteNo) == 7);

	// Not a voter (a spectator): no vote rows.
	v.voter = false;
	CHECK_FALSE(Shows(Core_Menu_Main(v), "Vote Yes"));
}

TEST_CASE("menu: Call a Vote only when a vote can be called")
{
	MenuView v = Voting(Ffa());
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::CallVote) > RowOf(Core_Menu_Main(v), MenuAction::MatchInfo));
	v.vote_can_call = false;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::CallVote) < 0);
}

TEST_CASE("menu: Call a Vote lists the allowed kinds; restart is called from it")
{
	MenuView v = Voting(Ffa());
	MenuPage page = Core_Menu_CallVote(v);
	CHECK(Actions(page) == std::vector<MenuAction>{MenuAction::VoteMaps, MenuAction::VoteGametypes, MenuAction::VoteFor,
	                                               MenuAction::Back});
	CHECK(Row(page, "Restart Match")->arg == "restart");
	v.vote_kinds = {"map"};
	CHECK(Actions(Core_Menu_CallVote(v)) == std::vector<MenuAction>{MenuAction::VoteMaps, MenuAction::Back});
}

TEST_CASE("menu: vote maps call the vote; the current one is marked and skipped")
{
	const MenuView v = Voting(Ffa()); // on q2dm1
	MenuPage maps = Core_Menu_VoteMaps(v);
	CHECK(maps[0].text == "Vote: Map");
	CHECK(Row(maps, "q2dm1")->action == MenuAction::None);
	CHECK(Row(maps, "q2dm1")->highlight);
	CHECK(Row(maps, "q2dm2")->action == MenuAction::VoteFor);
	CHECK(Row(maps, "q2dm2")->arg == "map q2dm2");
	CHECK(maps.back().action == MenuAction::CallVote); // Back to Call a Vote
}

TEST_CASE("menu: a gametype with variants opens them; one with a single ruleset is voted from the list")
{
	MenuView v = Voting(Ffa()); // FFA, ruleset ffa
	v.ruleset = "ffa";
	const MenuPage gts = Core_Menu_VoteGametypes(v);
	CHECK(gts[0].text == "Vote: Gametype");
	CHECK(Row(gts, "Free For All")->action == MenuAction::VoteVariants); // still selectable: pick Stock
	CHECK(Row(gts, "Free For All")->highlight);                          // but marked as current
	CHECK(Row(gts, "Duel")->action == MenuAction::VoteVariants);
	CHECK(Row(gts, "Duel")->arg == "duel");
	CHECK_FALSE(Row(gts, "Duel")->highlight);
	CHECK(Row(gts, "Team Deathmatch")->action == MenuAction::VoteFor);
	CHECK(Row(gts, "Team Deathmatch")->arg == "gametype tdm");
	CHECK(gts.back().action == MenuAction::CallVote);

	const MenuPage duel = Core_Menu_VoteVariants(v, "duel");
	CHECK(duel[0].text == "Vote: Duel");
	CHECK(Row(duel, "Duel")->arg == "gametype duel");
	CHECK(Row(duel, "HoonyMode")->arg == "gametype hoony");
	CHECK(duel.back().action == MenuAction::VoteGametypes); // Back to the gametypes

	const MenuPage ffa = Core_Menu_VoteVariants(v, "ffa");
	CHECK(Row(ffa, "Free For All")->action == MenuAction::None); // the current ruleset
	CHECK(Row(ffa, "Casual")->arg == "gametype ffa_casual");

	// Playing TDM: its single row is the current one.
	v.gametype_id = "tdm";
	v.ruleset = "tdm";
	CHECK(Row(Core_Menu_VoteGametypes(v), "Team Deathmatch")->action == MenuAction::None);
}

TEST_CASE("menu: a list longer than the page points to callvote")
{
	MenuView v = Voting(Ffa());
	v.vote_maps.clear();
	for (int i = 1; i <= 20; ++i)
		v.vote_maps.push_back("map" + std::to_string(i));
	const MenuPage page = Core_Menu_VoteMaps(v);
	CHECK(page.size() == (size_t)kMenuRows);
	CHECK(Shows(page, "map13"));
	CHECK_FALSE(Shows(page, "map14"));
	CHECK(Shows(page, "More: callvote map"));
}
