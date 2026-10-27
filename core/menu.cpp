// Pure part of the Tab menu: pages, text cleaning and stock's layout format.
// No engine access.
#include "core/menu.h"

namespace
{
void Add(MenuPage &page, std::string text, MenuAction action = MenuAction::None, MenuAlign align = MenuAlign::Left,
         bool highlight = false)
{
	page.push_back({std::move(text), align, action, highlight});
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

	if (v.teams)
	{
		// No player counts, here or on Join the Match: the client's
		// proportional font can't line them up (Chris, 2026-10-27). Your own team stays listed, in the alternate
		// colour and skipped by the cursor: stock has no grey.
		const auto team = [&](const char *name, int which, MenuAction join)
		{
			const std::string text = std::string("Join ") + name;
			if (v.team == which)
				Add(page, text, MenuAction::None, MenuAlign::Left, true);
			else
				Add(page, text, join);
		};
		team("Red", 1, MenuAction::JoinRed);
		team("Blue", 2, MenuAction::JoinBlue);
		if (v.team != 0)
			Add(page, "Spectate", MenuAction::Spectate);
	}
	else if (v.duel)
	{
		Add(page, "Duel Queue", MenuAction::Queue);
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

	Add(page, "");
	Add(page, "Match Info", MenuAction::MatchInfo);
	Footer(page, {"Close", MenuAlign::Left, MenuAction::Close, false});
	return page;
}

int Core_Menu_MainCursor(const MenuPage &page, const MenuView &v)
{
	MenuAction want = MenuAction::None;
	if (v.teams && v.team == 0 && v.red != v.blue)
		want = v.red < v.blue ? MenuAction::JoinRed : MenuAction::JoinBlue;
	for (int i = 0; i < (int)page.size(); ++i)
	{
		if (want != MenuAction::None && page[i].action == want)
			return i;
	}
	return Core_Menu_Next(page, -1);
}

MenuPage Core_Menu_MatchInfo(const MenuView &v)
{
	MenuPage page;
	Title(page, "Match Info", true);
	Add(page, "");
	Add(page, Core_Menu_Clean(v.gametype));
	Add(page, Core_Menu_Clean("Ruleset: " + v.ruleset));
	Add(page, Core_Menu_Clean("Map: " + v.map_file));
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
	// stock's localised "Return to Main Menu"
	Footer(page, {"$g_pc_return", MenuAlign::Left, MenuAction::Back, false});
	return page;
}
