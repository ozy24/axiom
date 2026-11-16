// The inactivity check's hook into id's code (core/inactivity.h), a marked
// one-line call.
#pragma once

struct edict_t;
struct usercmd_t;

// From ClientThink, each command, before a timeout can hold it: under a
// registered gametype in deathmatch, a playing human with no input for
// g_inactivity seconds is warned, then moved to spectate.
void Core_Inactivity_Think(edict_t *ent, const usercmd_t *ucmd);
