// Warmup's loadout (docs/design/match-flow.md): each marked one-line call
// into id's code. Under match_warmup_weapons 1, a player spawning in warmup
// gets every weapon the map places, with full ammo, and drops nothing when
// they die; the match going live respawns everyone with stock's loadout.
#pragma once

struct edict_t;

// From the end of PutClientInServer: in warmup, the map's weapons and full
// ammo for them, and the best of them in hand.
void Core_Warmup_Loadout(edict_t *ent);

// From player_die, in place of stock's TossClientWeapon: whether the dead
// player's weapon and powerups stay with them (warmup, with the loadout on),
// so the floor doesn't fill with drops.
bool Core_Warmup_KeepsWeapons(const edict_t *ent);
