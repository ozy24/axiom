// Location callouts' hooks into id's code (core/loc.h, decision 0027), each
// a marked one-line call. Deathmatch only: single player and coop are stock.
#pragma once

struct edict_t;

// From SpawnEntities once every entity has spawned: reads the level's
// <g_loc_dir>/<map>.loc (deathmatch).
void Core_Loc_LevelStart();

// `loc <text with %l, %h...>`; true if handled. From ClientCommand, before
// id's chain. With g_loc 0 (the compiled default) it is refused.
bool Core_Loc_ClientCommand(edict_t *ent, const char *cmd);
