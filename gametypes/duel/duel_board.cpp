// Pure part of Duel's scoreboard (duel_board.h). No engine access.
#include "gametypes/duel/duel_board.h"

#include "core/scoreboard.h"

#include <algorithm>

namespace
{
// Layout x, from the left of stock's 320-wide area. The duellists' blocks
// sit where stock's deathmatch board puts its first two (each a 32-wide
// picture, then the name, score and ping); the lists below use the `ctf`
// row, which draws its two numbers at x and x + 27 and the name at x + 54
// (the queue's: place and ping, under stock's ping icon).
constexpr int kBlockX[2] = {-72, 130};
constexpr int kTitleY = -40, kTimeY = -30, kWinnerY = -20, kListY = 44, kRowHeight = 8;
constexpr int kRowX = -40;
constexpr int kSpectatorX[2] = {-40, 160};

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

std::string Quoted(const std::string &s)
{
	return "\"" + s + "\"";
}
} // namespace

std::string Duel_BoardLayout(const DuelBoardInput &in)
{
	std::string out;
	const auto add = [&](const std::string &part)
	{
		if (out.size() + part.size() > in.budget)
			return false;
		out += part;
		return true;
	};

	if (!in.title.empty())
		add(Tokens("xv 0 yv", kTitleY, "cstring2", Quoted(in.title)));
	if (!in.match_time.empty())
		add(Tokens("xv 0 yv", kTimeY, "cstring2", Quoted("Match time " + in.match_time)));

	// The duellists: stock's blocks, a tick in ready-up, the winner marked.
	for (size_t i = 0; i < in.duellists.size() && i < 2; ++i)
	{
		const DuelBoardDuellist &d = in.duellists[i];
		const int x = kBlockX[i];
		std::string block = Tokens("xv", x + 32, "yv 0 dogtag", d.slot, "client", x, 0, d.slot, d.score,
		                           std::min(d.ping, 999), d.minutes);
		if (in.ready_up && d.ready)
			block += Tokens("xv", x + 16, "yv 16 picn wheel/p_compass_selected");
		if (d.slot == in.winner)
			block += Tokens("xv", x + 40, "yv", kWinnerY, "string2 WINNER");
		add(block);
	}

	// The queue, in order: each row its place and ping (your own row green:
	// the `ctf` row does that itself).
	int y = kListY;
	if (!in.queue.empty() && add(Tokens("xv", kRowX + 27, "yv", y, "picn ping xv", kRowX + 54, "string2 Queue")))
	{
		y += kRowHeight;
		for (size_t i = 0; i < in.queue.size(); ++i)
		{
			const DuelBoardWatcher &w = in.queue[i];
			if (!add(Tokens("ctf", kRowX, y, w.slot, (int)i + 1, std::clamp(w.ping, 0, 999), "\"\"")))
				return out;
			y += kRowHeight;
		}
		y += kRowHeight;
	}

	// Everyone else, two to a line: 0 and ping, as on the other boards.
	if (in.spectators.empty() || !add(Core_Scoreboard_SpectatorTitle(kSpectatorX[0], y)))
		return out;
	y += kRowHeight;
	for (size_t i = 0; i < in.spectators.size(); ++i)
	{
		const DuelBoardWatcher &w = in.spectators[i];
		if (!add(Core_Scoreboard_SpectatorRow(kSpectatorX[i % 2], y + kRowHeight * (int)(i / 2), w.slot, w.ping)))
			break;
	}
	return out;
}
