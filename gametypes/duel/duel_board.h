// Duel's scoreboard (g_scoreboard 1; docs/design/scoreboard.md), as pure
// logic: the layout text. No engine access; the adapter is duel_register.cpp.
//
//                   Duel on The Edge
//      [Xin's block]          [aid's block]       stock's player blocks
//
//       (ping) Queue                              (ping): stock's ping icon
//    1  17   OESD                                 the queue, in order: place, ping
//       (ping) Spectators
//    0  20   Tab         0  45   bizich           0, ping
//
// Names come only from the layout's `client` and `ctf` rows (a KEX name is
// a token the client decodes there alone), so the winner at intermission is
// marked over their block rather than named in a line.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct DuelBoardDuellist
{
	int slot = 0;
	int score = 0;
	int ping = 0;
	int minutes = 0;
	bool ready = false; // ready-up: ticked
};

struct DuelBoardWatcher
{
	int slot = 0;
	int ping = 0;
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

// The layout text, at most `budget` bytes.
std::string Duel_BoardLayout(const DuelBoardInput &in);
