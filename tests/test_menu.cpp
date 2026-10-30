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
	v.hostname = v.gametype = v.map = v.map_file = v.next_map = v.ruleset = std::string(60, 'W');
	v.red = v.blue = 99;
	v.team = 0;
	v.ready_up = v.ready_required = v.rounds = v.warmup = true;
	v.overtime = -1;
	v.capturelimit = v.fraglimit = 999;
	v.timelimit_min = 999;
	v.voter = true;
	v.vote_proposal = std::string(60, 'W');
	v.vote_kinds = {"map", "gametype", "restart", "readyall", "readyteam"};
	for (int i = 0; i < 40; ++i)
	{
		const std::string name = std::to_string(i) + std::string(30, 'W');
		v.vote_maps.push_back(name);
		MenuVoteGametype gt{name, name, {}};
		for (int r = 0; r < (i % 2 ? 1 : 40); ++r) // single rulesets and long variant lists
			gt.rulesets.emplace_back(name + std::to_string(r), name);
		v.vote_gametypes.push_back(gt);
	}
	for (const char *kind : {"timelimit", "scorelimit", "powerups", "friendlyfire"})
	{
		MenuVoteSetting setting{kind, std::string(60, 'W'), {}, "0"};
		for (int c = 0; c < 40; ++c)
			setting.choices.emplace_back(std::to_string(c), std::string(60, 'W'));
		v.vote_settings.push_back(setting);
	}
	v.captain = true;
	v.team_locked[1] = v.team_locked[2] = true;
	v.team_captain[1] = v.team_captain[2] = std::string(60, 'W');
	for (int i = 0; i < 40; ++i)
		v.teammates.emplace_back(i, std::string(60, 'W'));
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
	for (int state = 0; state < 8; ++state)
	{
		v.playing = state & 1;
		v.vote_open = state & 2;
		v.team = state & 4 ? 1 : 0; // on a team: Spectate shows too (with a vote and Ready, the longest)
		v.vote_can_call = !v.vote_open;
		for (const MenuPage &page :
		     {Core_Menu_Main(v), Core_Menu_MatchInfo(v), Core_Menu_CallVote(v), Core_Menu_VoteMaps(v),
		      Core_Menu_VoteGametypes(v), Core_Menu_VoteVariants(v, v.vote_gametypes[0].id),
		      Core_Menu_VoteSetting(v, "timelimit"), Core_Menu_Team(v), Core_Menu_TeamMate(v, 39)})
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
	const int about = RowOf(page, MenuAction::About);
	const int close = RowOf(page, MenuAction::Close);
	CHECK(Core_Menu_Next(page, -1) == join);
	CHECK(Core_Menu_Next(page, join) == chase);
	CHECK(Core_Menu_Next(page, chase) == info);
	CHECK(Core_Menu_Next(page, info) == about);
	CHECK(Core_Menu_Next(page, about) == close);
	CHECK(Core_Menu_Next(page, close) == join); // wraps
	CHECK(Core_Menu_Prev(page, join) == close);
	CHECK(Core_Menu_Next(MenuPage(3), -1) == -1);
}

TEST_CASE("menu cursor: a rebuilt page keeps the cursor on what it did, as rows move")
{
	MenuView v = Voting(Teams());
	v.team = 1;
	v.playing = v.ready_up = true;
	const MenuPage before = Core_Menu_Main(v);
	const int ready = RowOf(before, MenuAction::Ready);
	// A vote opens: five rows arrive above; the cursor stays on Ready.
	v.vote_open = true;
	v.vote_can_call = false;
	v.vote_proposal = "map q2dm3";
	const MenuPage during = Core_Menu_Main(v);
	REQUIRE(RowOf(during, MenuAction::Ready) != ready);
	CHECK(Core_Menu_Keep(during, ready, MenuAction::Ready, "") == RowOf(during, MenuAction::Ready));
	// Ready flips to Not Ready in place: the same row.
	v.ready = true;
	const MenuPage flipped = Core_Menu_Main(v);
	const int at = RowOf(during, MenuAction::Ready);
	CHECK(Core_Menu_Keep(flipped, at, MenuAction::Ready, "") == at);
	CHECK(flipped[at].action == MenuAction::NotReady);
	// A row that went (Call a Vote, while the vote is open): the first choice.
	CHECK(Core_Menu_Keep(during, 0, MenuAction::CallVote, "") == Core_Menu_Next(during, -1));
	// Choice rows match by their argument too.
	const MenuPage maps = Core_Menu_VoteMaps(v);
	int dm3 = -1;
	for (int i = 0; i < (int)maps.size(); ++i)
		if (maps[i].arg == "map q2dm3")
			dm3 = i;
	REQUIRE(dm3 > RowOf(maps, MenuAction::VoteFor)); // not the first choice
	CHECK(Core_Menu_Keep(maps, 0, MenuAction::VoteFor, "map q2dm3") == dm3);
}

TEST_CASE("menu: FFA spectator joins or chases; a player can spectate")
{
	MenuView v = Ffa();
	MenuPage page = Core_Menu_Main(v);
	CHECK(Actions(page) == std::vector<MenuAction>{MenuAction::JoinGame, MenuAction::Chase, MenuAction::MatchInfo,
	                                               MenuAction::About, MenuAction::Close});
	CHECK(Shows(page, "Join the Match"));
	v.chasing = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::StopChase) >= 0);
	v.playing = true;
	v.chasing = false;
	CHECK(Actions(Core_Menu_Main(v)) ==
	      std::vector<MenuAction>{MenuAction::Spectate, MenuAction::MatchInfo, MenuAction::About, MenuAction::Close});
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
	                                               MenuAction::About, MenuAction::Close});
}

TEST_CASE("menu: Duel offers joining or leaving the queue (the queue is on its scoreboard)")
{
	MenuView v = Ffa();
	v.duel = true;
	MenuPage page = Core_Menu_Main(v);
	CHECK(Actions(page) == std::vector<MenuAction>{MenuAction::JoinGame, MenuAction::Chase, MenuAction::MatchInfo,
	                                               MenuAction::About, MenuAction::Close});
	CHECK(Row(page, "Join Queue")->action == MenuAction::JoinGame);
	CHECK(Core_Menu_MainCursor(page, v) == RowOf(page, MenuAction::JoinGame)); // a new arrival's first choice
	v.queued = true;                                                           // waiting
	CHECK(Row(Core_Menu_Main(v), "Leave Queue")->action == MenuAction::Spectate);
	v.playing = true; // duelling
	CHECK(Actions(Core_Menu_Main(v)) ==
	      std::vector<MenuAction>{MenuAction::Spectate, MenuAction::MatchInfo, MenuAction::About, MenuAction::Close});
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
	CHECK_FALSE(Shows(page, "Next map: "));
	v.next_map = "q2ctf3";
	CHECK(Shows(Core_Menu_MatchInfo(v), "Next map: q2ctf3"));
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

TEST_CASE("menu: Call a Vote offers the ready votes in a ready-up; readyteam on a team")
{
	MenuView v = Voting(Teams());
	v.vote_kinds = {"readyall", "readyteam"};
	CHECK(Actions(Core_Menu_CallVote(v)) == std::vector<MenuAction>{MenuAction::Back});
	v.ready_up = true;
	CHECK(Row(Core_Menu_CallVote(v), "Ready Everyone")->arg == "readyall");
	CHECK_FALSE(Row(Core_Menu_CallVote(v), "Ready My Team"));
	v.team = 2;
	CHECK(Row(Core_Menu_CallVote(v), "Ready My Team")->arg == "readyteam");
	v.vote_kinds = {"readyteam"};
	CHECK_FALSE(Row(Core_Menu_CallVote(v), "Ready Everyone"));
}

TEST_CASE("menu: Call a Vote offers Shuffle Teams in a team game's warmup with three players")
{
	MenuView v = Voting(Teams());
	v.vote_kinds = {"shuffle"};
	v.red = 2;
	v.blue = 1;
	CHECK_FALSE(Row(Core_Menu_CallVote(v), "Shuffle Teams")); // live
	v.warmup = true;
	CHECK(Row(Core_Menu_CallVote(v), "Shuffle Teams")->arg == "shuffle");
	v.blue = 0;
	CHECK_FALSE(Row(Core_Menu_CallVote(v), "Shuffle Teams")); // two players: nothing to shuffle
	v.blue = 1;
	v.vote_kinds = {"map"};
	CHECK_FALSE(Row(Core_Menu_CallVote(v), "Shuffle Teams"));
	MenuView ffa = Voting(Ffa());
	ffa.vote_kinds = {"shuffle"};
	ffa.warmup = true;
	ffa.red = 3;
	CHECK_FALSE(Row(Core_Menu_CallVote(ffa), "Shuffle Teams"));
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

TEST_CASE("menu: setting votes follow the kinds, each opening its choices")
{
	MenuView v = Voting(Ffa());
	v.vote_settings = {
	    {"timelimit", "Time Limit", {{"5", "5 minutes"}, {"10", "10 minutes"}, {"20", "20 minutes"}}, "10"},
	    {"powerups", "Powerups", {{"on", "On"}, {"off", "Off"}}, "on"}};
	const MenuPage call = Core_Menu_CallVote(v);
	CHECK(Row(call, "Time Limit")->action == MenuAction::VoteSetting);
	CHECK(Row(call, "Time Limit")->arg == "timelimit");
	CHECK(Row(call, "Powerups")->arg == "powerups");
	CHECK(RowOf(call, MenuAction::VoteSetting) > RowOf(call, MenuAction::VoteFor)); // after Restart Match

	const MenuPage time = Core_Menu_VoteSetting(v, "timelimit");
	CHECK(time[0].text == "Vote: Time Limit");
	CHECK(Row(time, "10 minutes")->action == MenuAction::None); // the setting now: marked, skipped
	CHECK(Row(time, "10 minutes")->highlight);
	CHECK(Row(time, "20 minutes")->action == MenuAction::VoteFor);
	CHECK(Row(time, "20 minutes")->arg == "timelimit 20");
	CHECK(time.back().action == MenuAction::CallVote);
	CHECK(Row(Core_Menu_VoteSetting(v, "powerups"), "Off")->arg == "powerups off");
}

TEST_CASE("menu: a locked team always shows [LOCKED]; only its roster can choose it")
{
	MenuView v = Teams();
	v.team_locked[1] = true;
	v.team_may_join[1] = false;
	MenuPage page = Core_Menu_Main(v);
	CHECK(Shows(page, "Join Red [LOCKED]"));
	CHECK(RowOf(page, MenuAction::JoinRed) == -1);
	CHECK(Shows(page, "Join Blue"));
	CHECK(RowOf(page, MenuAction::JoinBlue) >= 0);

	v.team_may_join[1] = true; // a rostered player coming back: still marked
	page = Core_Menu_Main(v);
	CHECK(Row(page, "Join Red [LOCKED]")->action == MenuAction::JoinRed);

	// Your own team stays listed as yours, locked or not.
	v.team = 1;
	v.team_may_join[1] = false;
	page = Core_Menu_Main(v);
	REQUIRE(Row(page, "Join Red [LOCKED]"));
	CHECK(Row(page, "Join Red [LOCKED]")->highlight);
}

TEST_CASE("menu: match info names each team's captain and lock")
{
	MenuView v = Teams();
	v.team_captain[1] = "Ozy";
	v.team_locked[1] = true;
	const MenuPage page = Core_Menu_MatchInfo(v);
	CHECK(Shows(page, "Red [LOCKED]: Ozy"));
	CHECK(Shows(page, "Blue: none"));
	CHECK_FALSE(Shows(Core_Menu_MatchInfo(Ffa()), "Blue: none"));
}

TEST_CASE("menu: only a captain gets the Team page; it locks, readies and lists teammates")
{
	MenuView v = Teams();
	v.team = 1;
	v.playing = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::Team) == -1);
	v.captain = true;
	CHECK(RowOf(Core_Menu_Main(v), MenuAction::Team) >= 0);

	v.teammates = {{3, "Alpha"}, {5, "Bravo"}};
	MenuPage page = Core_Menu_Team(v);
	CHECK(RowOf(page, MenuAction::TeamLock) >= 0);
	CHECK(RowOf(page, MenuAction::TeamReady) == -1); // no ready-up now
	const MenuRow *bravo = Row(page, "Bravo");
	REQUIRE(bravo);
	CHECK(bravo->action == MenuAction::TeamMate);
	CHECK(bravo->arg == "5");

	v.team_locked[1] = true;
	v.ready_up = true;
	page = Core_Menu_Team(v);
	CHECK(RowOf(page, MenuAction::TeamUnlock) >= 0);
	CHECK(RowOf(page, MenuAction::TeamLock) == -1);
	CHECK(RowOf(page, MenuAction::TeamReady) >= 0);

	const MenuPage mate = Core_Menu_TeamMate(v, 5);
	CHECK(Row(mate, "Make Captain")->arg == "5");
	CHECK(Row(mate, "Kick from Team")->action == MenuAction::TeamKick);
	CHECK(mate.back().action == MenuAction::Team);
	// Gone from the team since the page was opened: nothing to do.
	CHECK(Actions(Core_Menu_TeamMate(v, 9)) == std::vector<MenuAction>{MenuAction::Team});
}

TEST_CASE("menu: a team too big for the page points to the console commands")
{
	MenuView v = Longest();
	v.team = 1;
	const MenuPage page = Core_Menu_Team(v);
	CHECK(Shows(page, "More: teamkick / captain"));
	CHECK(page.size() == (size_t)kMenuRows);
}

TEST_CASE("menu: About sits a blank line above Close, and shows the mod, its build and its page")
{
	MenuView v = Ffa();
	const MenuPage main = Core_Menu_Main(v);
	REQUIRE(main.size() == (size_t)kMenuRows);
	CHECK(main[kMenuRows - 3].action == MenuAction::About);
	CHECK(main[kMenuRows - 2].text.empty());
	CHECK(main[kMenuRows - 1].action == MenuAction::Close);
	v.mod_name = "Mod";
	v.mod_version = "1.2.3";
	v.mod_build = "abcd1234";
	v.mod_page = "https://github.com/someone/mod";
	const MenuPage about = Core_Menu_About(v);
	CHECK(Row(about, "Mod 1.2.3"));
	CHECK(Row(about, "build abcd1234"));
	CHECK(Row(about, "github.com/someone/mod"));
	CHECK(about.back().action == MenuAction::Back);
	v.mod_page.clear();
	CHECK_FALSE(Row(Core_Menu_About(v), "github.com/someone/mod"));
}

TEST_CASE("menu: Match Info for Clan Arena's elimination rounds")
{
	MenuView v = Teams();
	v.ctf = false;
	v.gametype = "Clan Arena";
	v.elimination = v.rounds = true;
	v.fraglimit = 8;
	v.round_lives = 1;
	v.round_time_s = 120;
	const MenuPage page = Core_Menu_MatchInfo(v);
	CHECK(Shows(page, "Rounds: first to 8"));
	CHECK(Shows(page, "Elimination: 1 life, 2:00"));
	CHECK_FALSE(Shows(page, "Fraglimit: 8"));
	CHECK_FALSE(Shows(page, "Rounds: a frag ends one"));
}
