// The narrow interface the mod's own logic uses to reach the engine.
//
// Logic modules under core/ and gametypes/ take a Host& instead of calling
// gi/level directly and never include g_local.h (scripts/check_core_seam.py),
// so unit tests drive them with a fake host (tests/fake_host.h) in
// milliseconds. Grow this interface only when a module needs something;
// core/host_gi.cpp is the one implementation over the engine.
#pragma once

#include <cstdint>
#include <string_view>

class Host
{
  public:
	virtual ~Host() = default;

	// Server console output, as gi.Com_Print.
	virtual void Print(std::string_view text) = 0;

	// Level time in milliseconds (level.time).
	virtual int64_t TimeMs() const = 0;
};

// The engine-backed host (core/host_gi.cpp). Valid from InitGame onwards.
Host &Core_GameHost();
