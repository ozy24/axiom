// Player-stat slot allocator for the mod's HUD (docs/design/gametype-api.md).
//
// Stock clients understand MAX_STATS (64) stat slots and read them only
// through the status-bar layout the server sends. Stock uses 0..STAT_LAST-1
// (pinned by checks/stock_client_contract.cpp), so the mod's own HUD values go
// in STAT_LAST..MAX_STATS-1 (54..63, ten slots).
//
// The CTF block (18..31) is NOT handed out: SetCTFStats writes it every frame
// in every mode. Outside team modes the miniscore (core/miniscore*) reuses
// its team-score stats, overwriting them after SetCTFStats.
//
// Allocation is by owner and idempotent: asking again with the same owner and
// count returns the same slots, so a game reload doesn't exhaust the pool.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class StatSlots
{
  public:
	// Hands out slots in [first, end).
	StatSlots(int first, int end);

	// `count` consecutive slots for `owner`, or nullopt if the pool can't fit
	// them (or the owner already holds a different count). Returns the first.
	std::optional<int> Allocate(std::string_view owner, int count = 1);

	// Slots still free.
	int Remaining() const;

	// "owner: first..last" per allocation, for the console and tests.
	std::vector<std::string> Describe() const;

  private:
	struct Range
	{
		std::string owner;
		int first;
		int count;
	};
	int first_, end_, next_;
	std::vector<Range> ranges_;
};

// The module's allocator over stock's free range (core/stat_slots_register.cpp).
StatSlots &Core_StatSlots();
