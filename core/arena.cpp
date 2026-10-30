// Pure part of the arena rules (core/arena.h). No engine access.
#include "core/arena.h"

#include <algorithm>

ArenaLoadout Core_Arena_Loadout(int health, int armor)
{
	// Every stock deathmatch weapon but the BFG, as Clan Arena gives them.
	ArenaLoadout l;
	l.weapons = {"weapon_blaster",        "weapon_shotgun",      "weapon_supershotgun",
	             "weapon_machinegun",     "weapon_chaingun",     "weapon_grenadelauncher",
	             "weapon_rocketlauncher", "weapon_hyperblaster", "weapon_railgun"};
	l.in_hand = "weapon_rocketlauncher";
	l.health = std::clamp(health, 1, 999);
	l.armor = std::clamp(armor, 0, 200);
	return l;
}

int DamageScore::Add(int attacker, int health_taken, int target_health, int armor_absorbed)
{
	const int dealt = std::clamp(health_taken, 0, std::max(0, target_health)) + std::max(0, armor_absorbed);
	int &left = remainder_[attacker];
	left += dealt;
	const int points = left / 100;
	left %= 100;
	return points;
}
