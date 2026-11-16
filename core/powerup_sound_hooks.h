// The powerup sounds' hook into id's code, a marked one-line call at each
// powerup's activation (Use_Quad, Use_QuadFire, Use_Invulnerability,
// Use_Invisibility, Use_Double).
#pragma once

struct edict_t;

// Plays a powerup's activation sound. In deathmatch under a registered
// gametype, every player hears it at full volume wherever they are, as
// MuffMode played it: the map knows someone has the Quad. Elsewhere
// (single player, coop, Rogue's stock gamerules) stock's: on the player's
// item channel, heard nearby.
void Core_Powerup_Sound(edict_t *ent, const char *sound);
