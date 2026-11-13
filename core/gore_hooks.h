// g_no_blood's hooks into id's code (core/gore.h), marked one-line calls.
#pragma once

#include <cstdint>

struct edict_t;

// From SpawnDamage: the temp entity to send for a hit (sparks for blood when
// g_no_blood is set; otherwise `type` unchanged).
int Core_Gore_HitEffect(int type);

// From ThrowGib and ThrowClientHead, once the gib is set up: hides a flesh
// gib, or takes the blood trail off a metal one, when g_no_blood is set.
// `gib_type` is id's gib_type_t (ThrowClientHead passes 0: flesh).
void Core_Gore_Gib(edict_t *gib, int32_t gib_type);
