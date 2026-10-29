// Duel's scoreboard (g_scoreboard 1; docs/design/scoreboard.md), as pure
// logic: the layout text, and each player's record this session. No engine
// access; the adapter is duel_register.cpp.
//
//                   Duel on The Edge
//      [Xin's block]          [aid's block]       stock's player blocks
//      W 3  L 1  streak 3     W 0  L 2            each duellist's record
//
//    W  L  Next up
//  1 0  2  OESD                                   the queue, in order
//  Spectators
//    0  0  Tab           1  4  bizich
//
// Names come only from the layout's `client` and `ctf` rows (a KEX name is
// a token the client decodes there alone), so the winner at intermission is
// marked over their block rather than named in a line.
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

struct DuelRecord
{
	int wins = 0;
	int losses = 0;
	int streak = 0; // wins in a row now
};

// Each client's wins and losses this session, by key (core/player_name.h).
class DuelRecords
{
  public:
	void Result(const std::string &winner, const std::string &loser);
	DuelRecord Get(const std::string &key) const;

  private:
	std::map<std::string, DuelRecord> records_;
};

struct DuelBoardDuellist
{
	int slot = 0;
	int score = 0;
	int ping = 0;
	int minutes = 0;
	bool ready = false; // ready-up: ticked
	DuelRecord record;
};

struct DuelBoardWatcher
{
	int slot = 0;
	DuelRecord record;
};

struct DuelBoardInput
{
	std::string title;                        // "Duel on The Edge" (no quotes)
	std::vector<DuelBoardDuellist> duellists; // up to two, the champion first
	std::vector<DuelBoardWatcher> queue;      // waiting to play, in order
	std::vector<DuelBoardWatcher> spectators; // the rest
	bool ready_up = false;                    // show the ready ticks
	int winner = -1;                          // intermission: the winner's slot
	std::string match_time;                   // intermission: "10:00"
	int viewer = -1;
	size_t budget = 980;
};

// The record under a duellist's block: "W 3  L 1  streak 3" (a streak of
// two or more), or empty before their first result.
std::string Duel_RecordText(const DuelRecord &r);

// The layout text, at most `budget` bytes.
std::string Duel_BoardLayout(const DuelBoardInput &in);
