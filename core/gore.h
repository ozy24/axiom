// g_no_blood (off by default): a game without blood, as pure logic. Hits on
// players and monsters show sparks instead of blood, and flesh gibs (a
// gibbed player's or monster's pieces and head) are not drawn; metal pieces
// stay, without their blood trail. The adapter is gore_register.cpp; the
// hooks into id's code are in gore_hooks.h.
#pragma once

// What a hit shows: `blood_effect` when id's code chose a blood effect
// (TE_BLOOD, TE_MOREBLOOD, TE_GREENBLOOD). True: show sparks instead.
bool Core_Gore_HitToSparks(bool no_blood, bool blood_effect);

enum class GibLook
{
	Stock,   // as id's code made it
	NoTrail, // drawn, without the blood trail (metal pieces)
	Hidden,  // not drawn at all (flesh)
};

// How a gib from ThrowGib or ThrowClientHead looks. `metallic` and `debris`
// are id's GIB_METALLIC and GIB_DEBRIS (debris never bleeds).
GibLook Core_Gore_GibLook(bool no_blood, bool metallic, bool debris);
