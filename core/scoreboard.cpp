// Pure part of the column scoreboard (core/scoreboard.h). No engine access.
#include "core/scoreboard.h"

#include <algorithm>

namespace
{
// Layout x, from the left of stock's 320-wide area. The `ctf` row draws the
// frags at its x, the deaths 27 to the right and the name 54 to the right
// (16 characters of 8 end at 142). The position goes left of it, where the
// team board draws a captain's `*`.
constexpr int kRankX = -64, kRowX = -40, kFphX = 166, kTimeX = 206, kPingX = 246;
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
				break;
			}
			used -= rows.back().size();
			rows.pop_back();
		}
	}
	for (const std::string &row : rows)
		out += row;

	// The spectators, two to a line, you first; as many as fit.
	if (watching.empty())
		return out;
	std::stable_partition(watching.begin(), watching.end(),
	                      [&](const ScoreboardPlayer &p) { return p.slot == in.viewer; });
	int y = kFirstRowY + kRowHeight * ((int)rows.size() + 1);
	const std::string title = Tokens("xv", kRowX, "yv", y, "loc_string2 0 \"$g_pc_spectators\"");
	if (out.size() + title.size() > in.budget)
		return out;
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
	return out;
}
