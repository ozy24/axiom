// The arena reset's movers (core/movers_register.cpp), called by the match
// flow's adapter.
#pragma once

// Every server frame: half a second into a deathmatch level, the movers'
// state is copied.
void Core_Movers_Frame();

// The arena reset: every mover back as it was then; returns how many.
int Core_Movers_Reset();
