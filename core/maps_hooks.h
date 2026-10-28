// The map rotation's hooks into id's code (each a marked one-line call) and
// what the Tab menu reads from it (docs/decisions/0014).
#pragma once

#include <string>

struct edict_t;

// From EndDMLevel, after stock's same-level and forced-map checks: with a
// g_map_list, begins the intermission towards the rotation's next map and
// returns true. False with no list (or under Rogue's stock gamerules
// modes): stock's rotation runs.
bool Core_Maps_EndLevel();

// Every server frame in deathmatch, from Core_Match_Frame: announces the
// next map as the intermission begins.
void Core_Maps_Frame();

// Where the level goes next, as EndDMLevel would choose now: for the Tab
// menu and `maplist`.
std::string Core_Maps_NextMap();

// `maplist` from a player, `sv maplist` from the console. True if handled.
bool Core_Maps_ClientCommand(edict_t *ent, const char *cmd);
bool Core_Maps_ServerCommand(const char *cmd);
