// The arena rules (match_arena; Clan Arena, docs/design/clan-arena.md), as
// pure logic: the spawn loadout, and the damage score's arithmetic. No engine
// access; the adapter is core/arena_register.cpp.
#pragma once

#include <map>
#include <string>
#include <vector>

// What every spawn gets: these weapons, each with its ammo at the player's
// stock maximum (never over it: MMREX gives 50 slugs against a cap of 25),
// health and body armor, and a weapon in hand.
struct ArenaLoadout
{
	std::vector<std::string> weapons; // classnames
	std::string in_hand;
	int health = 0;
	int armor = 0;
};

// The loadout for match_arena_health / match_arena_armor: health 1-999, armor
// 0-200 (body armor's most).
ArenaLoadout Core_Arena_Loadout(int health, int armor);

// The damage score (match_damage_score): a point per 100 damage dealt to
// enemies - health taken, capped at what the target had, plus armor absorbed.
// A remainder carries over (MMREX drops it each round and between rounds).
class DamageScore
{
  public:
	// `attacker` dealt this much; returns the whole points it makes now.
	int Add(int attacker, int health_taken, int target_health, int armor_absorbed);
	// Going LIVE, a new level: nothing carries over.
	void Reset()
	{
		remainder_.clear();
	}
	// A client left: their remainder goes.
	void Forget(int attacker)
	{
		remainder_.erase(attacker);
	}

  private:
	std::map<int, int> remainder_;
};
