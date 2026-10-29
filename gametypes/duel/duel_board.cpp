// Pure part of Duel's scoreboard (duel_board.h). No engine access.
#include "gametypes/duel/duel_board.h"

#include <algorithm>

namespace
{
// Layout x, from the left of stock's 320-wide area. The duellists' blocks
// sit where stock's deathmatch board puts its first two (each a 32-wide
// picture, then the name, score and ping); the lists below use the `ctf`
// row, which draws its two numbers at x and x + 27 and the name at x + 54.
constexpr int kBlockX[2] = {-72, 130};
constexpr int kTitleY = -40, kTimeY = -30, kWinnerY = -20, kRecordY = 36, kListY = 52, kRowHeight = 8;
constexpr int kPosX = -64, kRowX = -40;
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

// A list row: wins, losses and the name (the `ctf` row caps the second at 999).
std::string ListRow(int x, int y, const DuelBoardWatcher &w)
{
	return Tokens("ctf", x, y, w.slot, std::min(w.record.wins, 9999), std::min(w.record.losses, 999), "\"\"");
}
} // namespace

void DuelRecords::Result(const std::string &winner, const std::string &loser)
{
	DuelRecord &w = records_[winner];
	++w.wins;
	++w.streak;
	DuelRecord &l = records_[loser];
	++l.losses;
	l.streak = 0;
}

DuelRecord DuelRecords::Get(const std::string &key) const
{
	const auto it = records_.find(key);
	return it == records_.end() ? DuelRecord{} : it->second;
}

std::string Duel_RecordText(const DuelRecord &r)
{
	if (!r.wins && !r.losses)
		return {};
	std::string text = "W " + std::to_string(r.wins) + "  L " + std::to_string(r.losses);
	if (r.streak >= 2)
		text += "  streak " + std::to_string(r.streak);
	return text;
}

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

	// The duellists: stock's blocks, a tick in ready-up, the winner marked,
	// and each one's record underneath.
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
		if (const std::string record = Duel_RecordText(d.record); !record.empty())
			block += Tokens("xv", x + 40, "yv", kRecordY, d.slot == in.viewer ? "string2" : "string", Quoted(record));
		add(block);
	}

	// The queue, in order, numbered: who plays next.
	int y = kListY;
	if (!in.queue.empty() && add(Tokens("xv", kRowX, "yv", y, "string2 W xv", kRowX + 27, "string2 L xv", kRowX + 54,
	                                    "string2 \"Next up\"")))
	{
		y += kRowHeight;
		for (size_t i = 0; i < in.queue.size(); ++i)
		{
			const DuelBoardWatcher &w = in.queue[i];
			if (!add(ListRow(kRowX, y, w) +
			         Tokens("xv", kPosX, w.slot == in.viewer ? "string2" : "string", (int)i + 1)))
				return out;
			y += kRowHeight;
		}
		y += kRowHeight;
	}

	// Everyone else, two to a line, under the same W and L.
	if (in.spectators.empty() || !add(Tokens("xv", kRowX, "yv", y, "loc_string2 0 \"$g_pc_spectators\"")))
		return out;
	y += kRowHeight;
	for (size_t i = 0; i < in.spectators.size(); ++i)
		if (!add(ListRow(kSpectatorX[i % 2], y + kRowHeight * (int)(i / 2), in.spectators[i])))
			break;
	return out;
}
