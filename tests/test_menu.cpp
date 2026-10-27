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
	return v;
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
	MenuPage page = {{"Title", MenuAlign::Center, MenuAction::None, true},
	                 {},
	                 {"Go", MenuAlign::Left, MenuAction::Close, false},
	                 {"R", MenuAlign::Right, MenuAction::None, false}};
	CHECK(Core_Menu_Layout(page, 2) == "xv 32 yv 8 picn inventory "
	                                   "yv 32 xv 0 loc_cstring2 1 \"Title\" \"\" "
	                                   "yv 48 xv 64 loc_string2 1 \"Go\" \"\" xv 56 string2 \">\" "
	                                   "yv 56 xv 260 loc_rstring 1 \"R\" \"\" ");
}

TEST_CASE("menu pages fit stock's 18 rows and the client's 1024-byte layout")
{
	MenuView v = Longest();
	for (int playing = 0; playing < 2; ++playing)
	{
		v.playing = playing;
		for (const MenuPage &page : {Core_Menu_Main(v), Core_Menu_MatchInfo(v)})
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
	v.players = 3;
	MenuPage page = Core_Menu_Main(v);
	CHECK(Actions(page) ==
	      std::vector<MenuAction>{MenuAction::JoinGame, MenuAction::Chase, MenuAction::MatchInfo, MenuAction::Close});
	CHECK(Shows(page, "Join the Match (3)"));
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

TEST_CASE("menu: teams list both sides with counts; the cursor starts on the smaller team")
{
	MenuView v = Teams();
	v.red = 3;
	v.blue = 1;
	MenuPage page = Core_Menu_Main(v);
	CHECK(Shows(page, "Join Red  (3)"));
	CHECK(Shows(page, "Join Blue (1)"));
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
	CHECK(Shows(page, "Join Red  (2)"));
	CHECK(std::any_of(page.begin(), page.end(),
	                  [](const MenuRow &r) { return r.text == "Join Red  (2)" && r.highlight; }));
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
