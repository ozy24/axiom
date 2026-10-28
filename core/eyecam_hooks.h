// Eyecam's hooks into id's code (each a marked one-line call).
#pragma once

struct edict_t;

// From UpdateChaseCam, once the target is known: in deathmatch, for a
// spectator watching in first person, puts the view at the target's eye,
// hides the target's body from this spectator only, and returns true;
// stock's third-person placement is skipped. False: stock's chase cam.
bool Core_EyeCam_Place(edict_t *ent, edict_t *target);

// From the end of G_SetSpectatorStats, after the frame's own view, gun and
// blend: a first-person chaser gets the target's (gun, kick, blends, fov,
// underwater), so it sees what they see.
void Core_EyeCam_SpectatorView(edict_t *ent);

// From Entity_IsVisibleToPlayer, for SVF_INSTANCED entities: false for a
// chased player seen by their own first-person chaser.
bool Core_EyeCam_Visible(const edict_t *ent, const edict_t *viewer);

// From ClientThink: a chasing spectator's use button (+use) flips first and
// third person; true if it did.
bool Core_EyeCam_Use(edict_t *ent);

// From ClientCommand: `eyecam` flips the view; true if handled.
bool Core_EyeCam_Command(edict_t *ent, const char *cmd);

// From ClientDisconnect: the slot's next client starts in first person.
void Core_EyeCam_Disconnect(edict_t *ent);
