// Arriving players' hooks into id's code (each a marked one-line call) and
// into the mod's own (docs/design/joining.md).
#pragma once

struct edict_t;

// From ClientBeginDeathmatch, before stock's team assignment: where a client
// arriving goes. Spectating (FFA, Duel) or on no team, with the Tab menu
// opening; or into play, on their team from the last level.
void Core_Join_Begin(edict_t *ent);

// The decision Core_Join_Begin took for `ent`: into play (for Duel's queue).
bool Core_Join_Plays(const edict_t *ent);

// Every server frame, from Core_Match_Frame: remembers who is playing (for
// the next level) and opens the menu for new arrivals.
void Core_Join_Frame();

// `join` / `spectate`; true if handled. From ClientCommand, before id's
// chain (after the gametype's own, so Duel's queue answers them there).
bool Core_Join_ClientCommand(edict_t *ent, const char *cmd);

// The same from the Tab menu.
void Core_Join_Run(edict_t *ent, bool join);

// FFA and Duel: in or out of play, as the `spectator` userinfo key drives
// it. The key is written here, as PutClientInServer re-reads it.
void Core_Join_SetSpectator(edict_t *ent, bool spectator);
void Core_Join_SetSpectatorInfo(edict_t *ent, bool spectator); // the key alone
