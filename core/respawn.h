// Respawning in deathmatch under a registered gametype (decision 0030), as
// MuffMode ran it: a dead player may respawn by pressing fire after a
// second, and with g_dm_force_respawn 1 is respawned anyway once
// g_dm_force_respawn_time seconds have passed, so nobody can stay dead to
// keep a lead safe while the clock runs out. Stock instead reads
// g_dm_force_respawn_time as the wait before any respawn, pressed or not.
// Pure logic; the adapter is core/respawn_register.cpp.
#pragma once

#include <cstdint>

struct RespawnTimes
{
	int64_t may_ms = 0;    // pressing fire respawns from here
	int64_t forced_ms = 0; // respawned regardless from here; 0 = never
};

// For a death at `now_ms`: may after 1 second (stock's minimum); forced, with
// `force`, after `force_seconds` (at least that second, so 0 means 1).
RespawnTimes Core_Respawn_Times(int64_t now_ms, bool force, float force_seconds);
