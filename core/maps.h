// The map rotation (docs/decisions/0014), as pure logic. No engine access;
// the adapter is core/maps_register.cpp.
//
// One list per gametype, g_map_list, set in its ruleset's cfg: what rotates
// and what a map vote offers. Stock's EndDMLevel already walks it; this
// takes over three things:
//   - a map that isn't in the list (the server's starting map, an admin's
//     `map`) is followed by the list's first, where stock would follow the
//     map's own exit and might never join the rotation;
//   - with g_map_list_shuffle the first pass is shuffled too (stock plays it
//     in written order), and the admin's g_map_list is never rewritten: the
//     play order lives in its own cvar;
//   - a new pass is reshuffled so that no map comes back until half the
//     list has been played since it last did (Core_Maps_Reshuffle).
// With no list, stock's rotation (each map's own exit) runs unchanged.
#pragma once

#include <random>
#include <string>
#include <string_view>
#include <vector>

// The words of g_map_list as map names: lowercased, duplicates dropped, and
// only [a-z0-9_-] with single '/' between parts ("q64/dm1"), up to 32
// characters; anything else is dropped, so a name can never break a console
// command or climb out of maps/.
std::vector<std::string> Core_Maps_Words(std::string_view text);

// The play order for `list`: without `shuffle`, the written order. With it,
// `kept` (the order so far) if it holds exactly the same maps, else a random
// one that starts with `current` if it's listed, so the whole first pass
// still plays after it.
std::vector<std::string> Core_Maps_Order(const std::vector<std::string> &list, const std::vector<std::string> &kept,
                                         std::string_view current, bool shuffle, std::mt19937 &rng);

// A new pass after the one in `last` (just played, in order): random, except
// that no map plays again until at least half the list (rounded down) has
// been played since its last turn. Every map still plays once per pass.
// Lists of two or fewer come back unchanged (two maps alternate).
std::vector<std::string> Core_Maps_Reshuffle(const std::vector<std::string> &last, std::mt19937 &rng);

struct MapsNext
{
	std::string map;                // where the rotation goes after the current map
	std::vector<std::string> order; // the play order from then on (a new pass after the last map)
};

// After `current` in `order`: the next map; off the list, the first; after
// the last, the first of the next pass (with `shuffle` a reshuffle, seeded
// from the pass just played, so asking twice gives the same answer). An
// empty order gives an empty map.
MapsNext Core_Maps_Next(const std::vector<std::string> &order, std::string_view current, bool shuffle);
