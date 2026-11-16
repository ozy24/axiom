// Players who stop playing (g_inactivity, as MuffMode's): a playing human
// with no input for the set time is warned, then moved to spectate, so an
// absent player doesn't sit in a slot (or, with forced respawn, feed frags
// every few seconds). A timeout is the way to take a break. Pure logic; the
// adapter is core/inactivity_register.cpp.
#pragma once

#include <array>
#include <cstdint>

enum class InactivityEvent
{
	None,
	Warn, // kWarnMs before the move: tell them
	Move, // the time is up: to spectate
};

class Inactivity
{
  public:
	static constexpr int kSlots = 256;
	static constexpr int64_t kWarnMs = 10000;
	static constexpr int kMinSeconds = 15; // a shorter setting is this (MuffMode's floor)

	// Seconds of g_inactivity as used: 0 off, else at least kMinSeconds.
	static int Seconds(int setting);

	// The slot's clock starts again from `now_ms` (a new level, a player
	// entering, the world paused by a timeout, someone not playing).
	void Reset(int slot, int64_t now_ms);

	// A frame of the slot's input at `now_ms`: `active` if it pressed,
	// moved or turned. With `seconds` (as Seconds) the limit, returns Warn
	// once as the limit nears and Move once it's reached (then starts again).
	InactivityEvent Frame(int slot, int64_t now_ms, bool active, int seconds);

  private:
	struct Slot
	{
		int64_t since_ms = -1; // last input; -1 = not started
		bool warned = false;
	};
	std::array<Slot, kSlots> slots_{};
};

// Whether a frame's input counts as playing: any button, any movement, or
// the view turned more than a degree (yaw or pitch) since the last frame.
bool Core_Inactivity_Active(int buttons, float forward, float side, float up, const float angles[2],
                            const float last_angles[2]);
