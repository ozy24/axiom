// Where an arriving client goes (docs/design/joining.md), as pure logic. No
// engine access; the adapter is core/join_register.cpp.
//
// A human new to the server arrives spectating, with the Tab menu open
// (g_auto_join 0, the default); bots, the host of a listen server and, with
// g_auto_join 1, everyone go straight into play. Under KEX every human
// reconnects at each map change, so the players of the last level are
// remembered and come back as they were: playing, on the same team.
//
// The memory outlives the game module (`map` reloads it), so the adapter
// keeps it in a cvar: Encode / Decode, with each key shortened to a hash.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// A short, stable stand-in for a client key (core/player_name.h) in the
// cvar: 8 hex digits (FNV-1a).
std::string Core_Join_Hash(std::string_view key);

// The playing clients of a level: key hash -> team (0 none (FFA, Duel's
// queue), 1 red, 2 blue).
class JoinMemory
{
  public:
	void Set(const std::vector<std::pair<std::string, int>> &playing); // (key, team)
	std::string Encode() const;                                        // "1a2b3c4d:1 5e6f7a8b:0"
	static JoinMemory Decode(std::string_view text);                   // anything malformed is skipped

	// Whether `key` was playing, and on which team.
	bool Recall(const std::string &key, int &team) const;
	// Early in a level: `last`'s players not back yet (not in `present`, the
	// keys of every client here) are still remembered, so a map change
	// before they're back doesn't lose them.
	void KeepAbsent(const JoinMemory &last, const std::vector<std::string> &present);
	bool Empty() const
	{
		return teams_.empty();
	}

  private:
	std::unordered_map<std::string, int> teams_;
};

struct JoinArrival
{
	bool play = false; // straight into play; else spectating with the menu open
	int team = 0;      // team modes: 1 or 2 back on their team; 0 the smaller team
};

struct JoinWho
{
	bool bot = false;
	bool host = false; // a listen server's own player
	bool remembered = false;
	int team = 0; // the remembered team
};

// Where a client arriving goes: back as they were, else into play if
// they're a bot, the host or g_auto_join is on, else spectating.
JoinArrival Core_Join_Arrive(const JoinWho &who, bool auto_join);
