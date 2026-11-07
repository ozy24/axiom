// Jump's rules as pure logic (no engine), so they can be unit tested; the
// engine-facing half is jump_register.cpp. The base's own code splits the
// same way (core/*.cpp and core/*_register.cpp).
#pragma once

#include <cstdint>
#include <string>

// Whether a hit does damage: players never hurt anyone (themselves
// included) and falls don't hurt. The world's hazards (lava, trigger_hurt)
// still do, so a map can send a player back to the start.
bool Jump_Hurts(bool attacker_is_player, bool fall);

// A run's time for the console: "12.345 s"; "0.000 s" for less than none.
std::string Jump_TimeText(int64_t ms);
