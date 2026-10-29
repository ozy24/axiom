// Pure part of the column scoreboard (core/scoreboard.h). No engine access.
#include "core/scoreboard.h"

#include <algorithm>
#include <cstdlib>

namespace
{
// Layout x, from the left of stock's 320-wide area. The `ctf` row draws the
// frags at its x, the deaths 27 to the right and the name 54 to the right
// (16 characters of 8 end at 142). The position goes left of it, where the
// team board draws a captain's `*`.
constexpr int kRankX = -64, kRowX = -40, kFphX = 166, kTimeX = 206, kPingX = 246, kReadyX = 274;
// The team board's rows (stock's CTFScoreboardMessage) and their ready marks:
// just right of the captain's `*` (-64, 174), where MuffMode puts its tick.
constexpr int kTeamRowX[2] = {-40, 200}, kTeamReadyX[2] = {-56, 182}, kTeamFirstRowY = 42;
constexpr int kStampY = 8, kTitleY = 24, kFirstRowY = 36, kRowHeight = 8;
constexpr int kSpectatorX[2] = {-40, 160};
// The spectators' title and one line ("xv -40 yv 132 loc_string2 0 ..."
// and "ctf -40 140 31 0 999 \"\" "), with room to spare.
constexpr size_t kWatcherReserve = 80;

std::string Text(const char *s)
{
	return s;
}
std::string Text(const std::string &s)
{
	return s;
}
template <typename N> std::string Text(N n)
{
	return std::to_string(n);
}

// "a b c ": each value and a space, as layout tokens.
template <typename... T> std::string Tokens(const T &...values)
{
	std::string out;
	((out += Text(values) + " "), ...);
	return out;
}

// A player's row; `rank` their position on the board (1 the top).
std::string PlayerRow(const ScoreboardPlayer &p, int rank, int y, bool viewer)
{
	// The client caps the second number at 999; the text columns follow
	// suit, and ping is the client's own cap too. The `ctf` row sets the y
	// the text after it uses.
	const char *text = viewer ? "string2" : "string";
	return Tokens("ctf", kRowX, y, p.slot, p.frags, std::min(p.deaths, 999), "\"\"", "xv", kRankX, text, rank, "xv",
	              kFphX, text, std::min(Core_Scoreboard_Fph(p.frags, p.played_ms), 9999), "xv", kTimeX, text,
	              std::min<int64_t>(p.played_ms / 60000, 999), "xv", kPingX, text, std::min(p.ping, 999));
}

void Spectators(std::string &out, std::vector<ScoreboardPlayer> watching, int rows, const ScoreboardInput &in);
} // namespace

int Core_Scoreboard_Fph(int frags, int64_t played_ms)
{
	if (played_ms < 1000)
		return 0;
	return (int)((int64_t)frags * 3600000 / played_ms);
}

std::vector<ScoreboardPlayer> Core_Scoreboard_Order(std::vector<ScoreboardPlayer> players)
{
	std::stable_sort(players.begin(), players.end(),
	                 [](const ScoreboardPlayer &a, const ScoreboardPlayer &b)
	                 {
		                 if (a.frags != b.frags)
			                 return a.frags > b.frags;
		                 if (a.deaths != b.deaths)
			                 return a.deaths < b.deaths;
		                 return a.slot < b.slot;
	                 });
	return players;
}

std::string Core_Scoreboard_Layout(const ScoreboardInput &in)
{
	std::string out;
	if (!in.stamp.empty())
		out += Tokens("xv 0 yv", kStampY, "cstring2", "\"[" + in.stamp + "]\"");
	out += Tokens("xv", kRowX, "yv", kTitleY, "string2 Frg xv", kRowX + 27, "string2 Dth xv", kRowX + 54,
	              "string2 Player xv", kFphX, "string2 FPH xv", kTimeX, "string2 Time xv", kPingX, "string2 Ping");

	std::vector<ScoreboardPlayer> playing, watching;
	for (const ScoreboardPlayer &p : Core_Scoreboard_Order(in.players))
		(p.spectator ? watching : playing).push_back(p);

	// A spectator viewer keeps room for the spectators' title and their own
	// line: the players can't crowd them off their own board.
	const bool watcher =
	    std::any_of(watching.begin(), watching.end(), [&](const ScoreboardPlayer &p) { return p.slot == in.viewer; });
	const size_t player_budget = watcher && in.budget > kWatcherReserve ? in.budget - kWatcherReserve : in.budget;

	// The players from the top while they fit. If your own row is left out,
	// the last of them make room for it.
	std::vector<std::string> rows;
	std::vector<ScoreboardPlayer> shown; // each row's player, in step with rows
	size_t used = out.size();
	bool viewer_in = false;
	for (const ScoreboardPlayer &p : playing)
	{
		const std::string row =
		    PlayerRow(p, (int)rows.size() + 1, kFirstRowY + kRowHeight * (int)rows.size(), p.slot == in.viewer);
		if (used + row.size() > player_budget)
			break;
		used += row.size();
		rows.push_back(row);
		shown.push_back(p);
		viewer_in |= p.slot == in.viewer;
	}
	const auto mine =
	    std::find_if(playing.begin(), playing.end(), [&](const ScoreboardPlayer &p) { return p.slot == in.viewer; });
	if (!viewer_in && mine != playing.end())
	{
		while (!rows.empty())
		{
			const std::string row =
			    PlayerRow(*mine, (int)(mine - playing.begin()) + 1, kFirstRowY + kRowHeight * (int)rows.size(), true);
			if (used + row.size() <= player_budget)
			{
				used += row.size();
				rows.push_back(row);
				shown.push_back(*mine);
				break;
			}
			used -= rows.back().size();
			rows.pop_back();
			shown.pop_back();
		}
	}
	for (const std::string &row : rows)
		out += row;

	// The spectators, then the ready marks while there's room: a player's
	// row matters more than their mark.
	Spectators(out, watching, (int)rows.size(), in);
	if (!in.ready_up)
		return out;
	for (size_t i = 0; i < shown.size(); ++i)
	{
		const std::string mark = Core_Scoreboard_ReadyMark(kReadyX, kFirstRowY + kRowHeight * (int)i);
		if (shown[i].ready && out.size() + mark.size() <= in.budget)
			out += mark;
	}
	return out;
}

namespace
{
// The spectators, two to a line, you first; as many as fit.
void Spectators(std::string &out, std::vector<ScoreboardPlayer> watching, int rows, const ScoreboardInput &in)
{
	if (watching.empty())
		return;
	std::stable_partition(watching.begin(), watching.end(),
	                      [&](const ScoreboardPlayer &p) { return p.slot == in.viewer; });
	int y = kFirstRowY + kRowHeight * (rows + 1);
	const std::string title = Tokens("xv", kRowX, "yv", y, "loc_string2 0 \"$g_pc_spectators\"");
	if (out.size() + title.size() > in.budget)
		return;
	out += title;
	y += kRowHeight;
	for (size_t i = 0; i < watching.size(); ++i)
	{
		const ScoreboardPlayer &p = watching[i];
		const std::string row = Tokens("ctf", kSpectatorX[i % 2], y + kRowHeight * (int)(i / 2), p.slot, p.frags,
		                               std::min(p.ping, 999), "\"\"");
		if (out.size() + row.size() > in.budget)
			break;
		out += row;
	}
}
} // namespace

std::string Core_Scoreboard_ReadyMark(int x, int y)
{
	return Tokens("xv", x, "yv", y, "string2 R");
}

std::string Core_Scoreboard_TeamReady(std::string_view layout, const std::set<int> &ready, size_t max)
{
	// Stock's team rows: "ctf <x> <y> <client> ...", x -40 or 200, y from 42.
	// The spectators' rows use the same x but come after the "$g_pc_spectators"
	// title; their clients are never ready (not playing), but stop there anyway.
	std::string out;
	const size_t end = std::min(layout.find("$g_pc_spectators"), layout.size());
	for (size_t at = layout.find("ctf "); at < end; at = layout.find("ctf ", at + 4))
	{
		if (at > 0 && layout[at - 1] != ' ')
			continue; // part of a longer token
		const std::string row(layout.substr(at + 4, 32));
		char *next = nullptr;
		const long x = std::strtol(row.c_str(), &next, 10);
		const long y = std::strtol(next, &next, 10);
		const long client = std::strtol(next, &next, 10);
		if ((x != kTeamRowX[0] && x != kTeamRowX[1]) || y < kTeamFirstRowY || !ready.count((int)client))
			continue;
		const std::string mark = Core_Scoreboard_ReadyMark(kTeamReadyX[x == kTeamRowX[1]], (int)y);
		if (layout.size() + out.size() + mark.size() < max)
			out += mark;
	}
	return out;
}
