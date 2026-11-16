// Pure part of the inactivity check (core/inactivity.h). No engine access.
#include "core/inactivity.h"

#include <algorithm>
#include <cmath>

int Inactivity::Seconds(int setting)
{
	if (setting <= 0)
		return 0;
	return std::max(setting, kMinSeconds);
}

void Inactivity::Reset(int slot, int64_t now_ms)
{
	if (slot < 0 || slot >= kSlots)
		return;
	slots_[slot] = {now_ms, false};
}

InactivityEvent Inactivity::Frame(int slot, int64_t now_ms, bool active, int seconds)
{
	if (slot < 0 || slot >= kSlots)
		return InactivityEvent::None;
	Slot &s = slots_[slot];
	if (seconds <= 0 || active || s.since_ms < 0 || now_ms < s.since_ms)
	{
		s = {now_ms, false};
		return InactivityEvent::None;
	}
	const int64_t limit = (int64_t)seconds * 1000;
	const int64_t idle = now_ms - s.since_ms;
	if (idle >= limit)
	{
		s = {now_ms, false};
		return InactivityEvent::Move;
	}
	if (!s.warned && idle >= limit - kWarnMs)
	{
		s.warned = true;
		return InactivityEvent::Warn;
	}
	return InactivityEvent::None;
}

bool Core_Inactivity_Active(int buttons, float forward, float side, float up, const float angles[2],
                            const float last_angles[2])
{
	if (buttons || forward != 0 || side != 0 || up != 0)
		return true;
	for (int i = 0; i < 2; ++i)
	{
		float d = std::fmod(std::fabs(angles[i] - last_angles[i]), 360.0f);
		if (d > 180.0f)
			d = 360.0f - d;
		if (d > 1.0f) // ignore a resting mouse's jitter
			return true;
	}
	return false;
}
