// Respawning's hooks into id's code (core/respawn.h), each a marked
// one-line call.
#pragma once

struct edict_t;

// From player_die, after stock has set respawn_time: under a registered
// gametype in deathmatch, pressing fire works after a second whatever
// g_dm_force_respawn_time says, and the forced respawn's time is kept.
void Core_Respawn_Died(edict_t *self);

// From ClientThink, once respawn_time has passed, in place of stock's
// `deathmatch && g_dm_force_respawn`: whether the dead player is respawned
// now without pressing. Stock's test outside a registered gametype.
bool Core_Respawn_Forced(const edict_t *ent);
