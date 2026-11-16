// Pure part of respawning (core/respawn.h). No engine access.
#include "core/respawn.h"

#include <algorithm>

RespawnTimes Core_Respawn_Times(int64_t now_ms, bool force, float force_seconds)
{
	constexpr int64_t kMinMs = 1000; // stock's second before a respawn
	RespawnTimes t;
	t.may_ms = now_ms + kMinMs;
	if (force)
	{
		// A nan or negative time is the minimum; an absurd one is capped at an hour.
		const float s = force_seconds > 0 ? std::min(force_seconds, 3600.0f) : 0.0f;
		t.forced_ms = std::max(t.may_ms, now_ms + (int64_t)(s * 1000));
	}
	return t;
}
