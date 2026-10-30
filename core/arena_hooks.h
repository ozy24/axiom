// The arena rules' hooks into id's code (each a marked one-line call):
// Clan Arena's loadout, no map items, no self damage, the damage score
// (docs/design/clan-arena.md).
#pragma once

struct edict_t;
struct gitem_t;

// match_arena is on (deathmatch, a registered gametype).
bool Core_Arena_On();

// From the end of PutClientInServer, after the warmup loadout: the arena
// loadout (every weapon but the BFG, full ammo, match_arena_health and
// match_arena_armor, the rocket launcher in hand).
void Core_Arena_Loadout(edict_t *ent);

// From SpawnItem's deathmatch block: true if the item mustn't spawn (the
// arena: every item but keys and CTF flags).
bool Core_Arena_NoItem(const gitem_t *item);

// From T_Damage, beside the friendly-fire check: true if this hit costs no
// health or armor (g_dm_no_self_damage: your own hits; knockback still
// applies, a telefrag still kills).
bool Core_Arena_NoSelfDamage(const edict_t *targ, const edict_t *attacker, int dflags);

// From T_Damage, by the hit marker: health taken, power armor and armor
// absorbed (not godmode), before the target's health goes down. The damage
// score (match_damage_score) in an elimination round being played.
void Core_Arena_Damage(edict_t *targ, edict_t *attacker, int take, int psave, int asave);

// The match going LIVE (the arena reset) and a client leaving: the damage
// score's remainders.
void Core_Arena_ResetDamage();
void Core_Arena_Disconnect(edict_t *ent);
