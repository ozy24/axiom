// Pure part of the Tab menu: pages, text cleaning and stock's layout format.
// No engine access.
#include "core/menu.h"

#include <algorithm>

namespace
{
void Add(MenuPage &page, std::string text, MenuAction action = MenuAction::None, MenuAlign align = MenuAlign::Left,
         bool highlight = false, std::string arg = {})
{
	page.push_back({std::move(text), align, action, highlight, std::move(arg)});
}

// A choice that is already the case (your team, your answer, the current
// map): listed, in the alternate colour, skipped by the cursor.
void Choice(MenuPage &page, std::string text, MenuAction action, bool current, std::string arg = {})
{
	if (current)
		Add(page, std::move(text), MenuAction::None, MenuAlign::Left, true);
	else
		Add(page, std::move(text), action, MenuAlign::Left, false, std::move(arg));
}

void Title(MenuPage &page, const std::string &text, bool highlight)
{
	Add(page, Core_Menu_Clean(text), MenuAction::None, MenuAlign::Center, highlight);
}

// Pads with blank rows to kMenuRows, then puts `last` on the bottom row.
void Footer(MenuPage &page, MenuRow last)
{
	while ((int)page.size() < kMenuRows - 1)
		page.push_back({});
	page.push_back(std::move(last));
}

std::string Minutes(int min)
{
	return min > 0 ? std::to_string(min) + " min" : "none";
}
} // namespace

std::string Core_Menu_Clean(std::string_view text)
{
	std::string out;
	for (const char c : text)
	{
		const unsigned char u = (unsigned char)c;
		if (u < 0x20 || u == 0x7f)
			continue;
		switch (c)
		{
		case '"':
			out += '\'';
			break;
		case '\\':
			out += '/';
			break;
		case '{':
			out += '(';
			break;
		case '}':
			out += ')';
			break;
		default:
			out += c;
		}
	}
	while (!out.empty() && (out[0] == '*' || out[0] == '$'))
		out.erase(0, 1);
	if (out.size() > kMenuLineChars)
		out = out.substr(0, kMenuLineChars - 3) + "...";
	return out;
}

std::string Core_Menu_Layout(const MenuPage &page, int cur)
{
	// PMenu_Do_Update, rerelease/ctf/p_ctf_menu.cpp, with statusbar_t's
	// spacing (g_statusbar.h): each token pair is followed by a space.
	std::string sb = "xv 32 yv 8 picn inventory ";
	for (int i = 0; i < (int)page.size(); ++i)
	{
		const MenuRow &row = page[i];
		if (row.text.empty())
			continue;
		sb += "yv " + std::to_string(32 + i * 8) + " ";
		const char *func = "loc_string";
		int x = 64;
		if (row.align == MenuAlign::Center)
			x = 0, func = "loc_cstring";
		else if (row.align == MenuAlign::Right)
			x = 260, func = "loc_rstring";
		sb += "xv " + std::to_string(x) + " ";
		sb += func;
		if (cur == i || row.highlight)
			sb += '2';
		sb += " 1 \"" + row.text + "\" \"\" ";
		if (cur == i)
			sb += "xv 56 string2 \">\" ";
	}
	return sb;
}

int Core_Menu_Next(const MenuPage &page, int cur)
{
	const int n = (int)page.size();
	for (int step = 1; step <= n; ++step)
	{
		const int i = ((cur < 0 ? -1 : cur) + step) % n;
		if (page[i].action != MenuAction::None)
			return i;
	}
	return -1;
}

int Core_Menu_Keep(const MenuPage &page, int cur, MenuAction action, std::string_view arg)
{
	if (action != MenuAction::None)
		for (int i = 0; i < (int)page.size(); ++i)
			if (page[i].action == action && page[i].arg == arg)
				return i;
	if (cur >= 0 && cur < (int)page.size() && page[cur].action != MenuAction::None)
		return cur;
	return Core_Menu_Next(page, -1);
}

int Core_Menu_Prev(const MenuPage &page, int cur)
{
	const int n = (int)page.size();
	for (int step = 1; step <= n; ++step)
	{
		const int i = (((cur < 0 ? 0 : cur) - step) % n + n) % n;
		if (page[i].action != MenuAction::None)
			return i;
	}
	return -1;
}

MenuPage Core_Menu_Main(const MenuView &v)
{
	MenuPage page;
	Title(page, v.hostname, true);
	Title(page, v.gametype, false);
	Title(page, v.map, true);
	Add(page, "");

	if (v.vote_open && v.voter)
	{
		std::string what = v.vote_proposal; // "map q2dm3": shown "Vote: Map q2dm3"
		if (!what.empty())
			what[0] = (char)toupper((unsigned char)what[0]);
		Add(page, Core_Menu_Clean("Vote: " + what), MenuAction::None, MenuAlign::Left, true);
		Add(page, "");
		Choice(page, "Vote Yes", MenuAction::VoteYes, v.vote_answer == 1);
		Choice(page, "Vote No", MenuAction::VoteNo, v.vote_answer == 0);
		Add(page, "");
	}

	if (v.teams)
	{
		// No player counts, here or on Join the Match: the client's
		// proportional font can't line them up (Chris, 2026-10-27). Your own team stays listed, in the alternate
		// colour and skipped by the cursor: stock has no grey.
		// A locked team always says so (Chris, 2026-10-29); one you can't
		// join (not on its roster) is skipped too.
		const auto team = [&](const char *name, int which, MenuAction join)
		{
			const std::string text = std::string("Join ") + name + (v.team_locked[which] ? " [LOCKED]" : "");
			if (v.team != which && !v.team_may_join[which])
				Add(page, text);
			else
				Choice(page, text, join, v.team == which);
		};
		team("Red", 1, MenuAction::JoinRed);
		team("Blue", 2, MenuAction::JoinBlue);
		if (v.team != 0)
			Add(page, "Spectate", MenuAction::Spectate);
	}
	else if (v.duel)
	{
		// The queue itself is on Duel's scoreboard (and `queue` prints it).
		Add(page, v.queued ? "Leave Queue" : "Join Queue", v.queued ? MenuAction::Spectate : MenuAction::JoinGame);
	}
	else if (v.playing)
	{
		Add(page, "Spectate", MenuAction::Spectate);
	}
	else
	{
		Add(page, "Join the Match", MenuAction::JoinGame);
	}

	if (!v.playing)
		Add(page, v.chasing ? "Stop Chasing" : "Chase Camera", v.chasing ? MenuAction::StopChase : MenuAction::Chase);
	if (v.ready_up && v.playing)
		Add(page, v.ready ? "Not Ready" : "Ready", v.ready ? MenuAction::NotReady : MenuAction::Ready);
	if (v.captain)
		Add(page, "Team (captain)", MenuAction::Team);

	Add(page, "");
	Add(page, "Match Info", MenuAction::MatchInfo);
	if (v.vote_can_call)
		Add(page, "Call a Vote", MenuAction::CallVote);
	// About sits at the bottom, just above Close.
	while ((int)page.size() < kMenuRows - 2)
		page.push_back({});
	Add(page, "About", MenuAction::About);
	Footer(page, {"Close", MenuAlign::Left, MenuAction::Close, false, {}});
	return page;
}

int Core_Menu_MainCursor(const MenuPage &page, const MenuView &v)
{
	MenuAction want = MenuAction::None;
	if (v.vote_open && v.voter && v.vote_answer < 0)
		want = MenuAction::VoteYes;
	else if (v.teams && v.team == 0 && v.red != v.blue)
		want = v.red < v.blue ? MenuAction::JoinRed : MenuAction::JoinBlue;
	for (int i = 0; i < (int)page.size(); ++i)
	{
		if (want != MenuAction::None && page[i].action == want)
			return i;
	}
	return Core_Menu_Next(page, -1);
}

MenuPage Core_Menu_About(const MenuView &v)
{
	MenuPage page;
	Title(page, "About", true);
	Add(page, "");
	Add(page, Core_Menu_Clean(v.mod_name + " " + v.mod_version), MenuAction::None, MenuAlign::Center);
	Add(page, Core_Menu_Clean("build " + v.mod_build), MenuAction::None, MenuAlign::Center);
	if (!v.mod_page.empty())
	{
		// The page without its scheme: shorter, and how people type it.
		std::string link = v.mod_page;
		for (const char *scheme : {"https://", "http://"})
			if (link.rfind(scheme, 0) == 0)
				link = link.substr(std::string(scheme).size());
		Add(page, "");
		Add(page, Core_Menu_Clean(link), MenuAction::None, MenuAlign::Center, true);
	}
	Footer(page, {"$g_pc_return", MenuAlign::Left, MenuAction::Back, false, {}});
	return page;
}

MenuPage Core_Menu_MatchInfo(const MenuView &v)
{
	MenuPage page;
	Title(page, "Match Info", true);
	Add(page, "");
	Add(page, Core_Menu_Clean(v.gametype));
	if (!v.ruleset.empty())
		Add(page, Core_Menu_Clean("Ruleset: " + v.ruleset));
	Add(page, Core_Menu_Clean("Map: " + v.map_file));
	if (!v.next_map.empty())
		Add(page, Core_Menu_Clean("Next map: " + v.next_map));
	Add(page, "");
	Add(page, "Timelimit: " + Minutes(v.timelimit_min));
	if (v.ctf)
		Add(page, "Capturelimit: " + (v.capturelimit > 0 ? std::to_string(v.capturelimit) : std::string("none")));
	else
		Add(page, "Fraglimit: " + (v.fraglimit > 0 ? std::to_string(v.fraglimit) : std::string("none")));
	if (v.match_flow)
	{
		Add(page, "Overtime: " + (v.overtime < 0 ? std::string("sudden death") : Minutes(v.overtime)));
		Add(page, std::string("Ready-up: ") + (v.ready_required ? "on" : "off"));
		if (v.rounds)
			Add(page, "Rounds: a frag ends one");
	}
	else
	{
		Add(page, "No warmup: stock rules");
	}
	if (v.teams)
	{
		Add(page, "");
		const auto team = [&](const char *name, int which)
		{
			const std::string captain = v.team_captain[which].empty() ? "none" : v.team_captain[which];
			Add(page, Core_Menu_Clean(std::string(name) + (v.team_locked[which] ? " [LOCKED]: " : ": ") + captain));
		};
		team("Red", 1);
		team("Blue", 2);
	}
	// stock's localised "Return to Main Menu"
	Footer(page, {"$g_pc_return", MenuAlign::Left, MenuAction::Back, false, {}});
	return page;
}

MenuPage Core_Menu_CallVote(const MenuView &v)
{
	MenuPage page;
	Title(page, "Call a Vote", true);
	Add(page, "");
	const auto allowed = [&](const char *kind)
	{ return std::find(v.vote_kinds.begin(), v.vote_kinds.end(), kind) != v.vote_kinds.end(); };
	if (allowed("map"))
		Add(page, "Map", MenuAction::VoteMaps);
	if (allowed("gametype"))
		Add(page, "Gametype", MenuAction::VoteGametypes);
	if (allowed("restart"))
		Add(page, "Restart Match", MenuAction::VoteFor, MenuAlign::Left, false, "restart");
	for (const MenuVoteSetting &s : v.vote_settings)
		Add(page, Core_Menu_Clean(s.title), MenuAction::VoteSetting, MenuAlign::Left, false, s.kind);
	// In a ready-up warmup: ready everyone playing, or your own team.
	if (v.ready_up && allowed("readyall"))
		Add(page, "Ready Everyone", MenuAction::VoteFor, MenuAlign::Left, false, "readyall");
	if (v.ready_up && v.team && allowed("readyteam"))
		Add(page, "Ready My Team", MenuAction::VoteFor, MenuAlign::Left, false, "readyteam");
	Footer(page, {"$g_pc_return", MenuAlign::Left, MenuAction::Back, false, {}});
	return page;
}

namespace
{
// Choice rows from row 2; the bottom row is `back`. A list longer than the
// page ends in `more`.
MenuPage ChoicePage(const std::string &title, const std::vector<MenuRow> &rows, const std::string &more, MenuRow back)
{
	MenuPage page;
	Title(page, title, true);
	Add(page, "");
	// 14 rows, not the 15 that fit: 15 full-length rows under a full-length
	// title pass the client's 1024-byte layout (tests/test_menu.cpp).
	constexpr size_t kRoom = kMenuRows - 4;
	const bool over = rows.size() > kRoom;
	const size_t shown = over ? kRoom - 1 : rows.size();
	for (size_t i = 0; i < shown; ++i)
		page.push_back(rows[i]);
	if (over)
		Add(page, Core_Menu_Clean(more));
	Footer(page, std::move(back));
	return page;
}

MenuRow ChoiceRow(const std::string &text, MenuAction action, bool current, std::string arg)
{
	MenuPage one;
	Choice(one, Core_Menu_Clean(text), action, current, std::move(arg));
	return one.front();
}
} // namespace

MenuPage Core_Menu_VoteMaps(const MenuView &v)
{
	std::vector<MenuRow> rows;
	for (const std::string &m : v.vote_maps)
		rows.push_back(ChoiceRow(m, MenuAction::VoteFor, m == v.map_file, "map " + m));
	return ChoicePage("Vote: Map", rows, "More: callvote map",
	                  {"Back", MenuAlign::Left, MenuAction::CallVote, false, {}});
}

MenuPage Core_Menu_VoteGametypes(const MenuView &v)
{
	std::vector<MenuRow> rows;
	for (const MenuVoteGametype &gt : v.vote_gametypes)
	{
		if (gt.rulesets.empty())
			continue;
		if (gt.rulesets.size() == 1)
		{
			const std::string &name = gt.rulesets.front().first;
			rows.push_back(ChoiceRow(gt.title, MenuAction::VoteFor, gt.id == v.gametype_id, "gametype " + name));
			continue;
		}
		// Its variants are a page of their own; the current gametype is marked
		// but stays selectable, to pick another of its variants.
		MenuRow row = ChoiceRow(gt.title, MenuAction::VoteVariants, false, gt.id);
		row.highlight = gt.id == v.gametype_id;
		rows.push_back(row);
	}
	return ChoicePage("Vote: Gametype", rows, "More: callvote gametype",
	                  {"Back", MenuAlign::Left, MenuAction::CallVote, false, {}});
}

MenuPage Core_Menu_VoteVariants(const MenuView &v, std::string_view gametype)
{
	std::vector<MenuRow> rows;
	std::string title = "Vote: Gametype";
	for (const MenuVoteGametype &gt : v.vote_gametypes)
	{
		if (gt.id != gametype)
			continue;
		title = "Vote: " + gt.title;
		for (const auto &[name, text] : gt.rulesets)
			rows.push_back(
			    ChoiceRow(text, MenuAction::VoteFor, gt.id == v.gametype_id && name == v.ruleset, "gametype " + name));
	}
	return ChoicePage(title, rows, "More: callvote gametype",
	                  {"Back", MenuAlign::Left, MenuAction::VoteGametypes, false, {}});
}

MenuPage Core_Menu_VoteSetting(const MenuView &v, std::string_view kind)
{
	std::vector<MenuRow> rows;
	std::string title = "Call a Vote";
	std::string more;
	for (const MenuVoteSetting &s : v.vote_settings)
	{
		if (s.kind != kind)
			continue;
		title = "Vote: " + s.title;
		more = "More: callvote " + s.kind;
		for (const auto &[arg, text] : s.choices)
			rows.push_back(ChoiceRow(text, MenuAction::VoteFor, arg == s.current, s.kind + " " + arg));
	}
	return ChoicePage(title, rows, more, {"Back", MenuAlign::Left, MenuAction::CallVote, false, {}});
}

MenuPage Core_Menu_Team(const MenuView &v)
{
	MenuPage page;
	Title(page, v.team == 1 ? "Red Team" : "Blue Team", true);
	Add(page, "");
	if (v.team_locked[v.team])
		Add(page, "Unlock Team", MenuAction::TeamUnlock);
	else
		Add(page, "Lock Team", MenuAction::TeamLock);
	if (v.ready_up)
		Add(page, "Ready Team", MenuAction::TeamReady);
	Add(page, "");
	Add(page, "Teammates:");
	// Rows left above the footer; a team too big for the page says so.
	const int room = kMenuRows - 1 - (int)page.size();
	const bool more = (int)v.teammates.size() > room;
	for (int i = 0; i < (int)v.teammates.size() && i < (more ? room - 1 : room); ++i)
		Add(page, Core_Menu_Clean(v.teammates[i].second), MenuAction::TeamMate, MenuAlign::Left, false,
		    std::to_string(v.teammates[i].first));
	if (more)
		Add(page, "More: teamkick / captain");
	if (v.teammates.empty())
		Add(page, "(none)");
	Footer(page, {"$g_pc_return", MenuAlign::Left, MenuAction::Back, false, {}});
	return page;
}

MenuPage Core_Menu_TeamMate(const MenuView &v, int client)
{
	MenuPage page;
	std::string name;
	for (const auto &[c, n] : v.teammates)
	{
		if (c == client)
			name = n;
	}
	Title(page, name.empty() ? "Not on your team" : name, true);
	Add(page, "");
	if (!name.empty())
	{
		const std::string arg = std::to_string(client);
		Add(page, "Make Captain", MenuAction::TeamCaptain, MenuAlign::Left, false, arg);
		Add(page, "Kick from Team", MenuAction::TeamKick, MenuAlign::Left, false, arg);
	}
	Footer(page, {"Back", MenuAlign::Left, MenuAction::Team, false, {}});
	return page;
}
