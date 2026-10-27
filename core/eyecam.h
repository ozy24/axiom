// Eyecam: a spectator chasing a player sees through their eyes, or from
// stock's third-person chase cam (docs/design/eyecam.md). Pure: who chose
// which. The adapter is core/eyecam_register.cpp.
#pragma once

#include <bitset>

class EyeCamPrefs
{
  public:
	static constexpr int kSlots = 256; // MAX_CLIENTS

	// First person unless the client in `slot` chose third.
	bool FirstPerson(int slot) const
	{
		return !InRange(slot) || !third_person_[slot];
	}
	// Flips the slot's view; returns the new one (true: first person).
	bool Toggle(int slot)
	{
		if (!InRange(slot))
			return true;
		third_person_.flip(slot);
		return !third_person_[slot];
	}
	// The client left: the next one in the slot starts in first person.
	void Forget(int slot)
	{
		if (InRange(slot))
			third_person_.reset(slot);
	}

  private:
	static bool InRange(int slot)
	{
		return slot >= 0 && slot < kSlots;
	}
	std::bitset<kSlots> third_person_;
};
