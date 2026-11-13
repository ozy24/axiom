// Engine-facing part of g_no_blood (core/gore.h): the cvar and the edicts.
#include "g_local.h"

#include "core/gore.h"
#include "core/gore_hooks.h"

namespace
{
cvar_t *g_no_blood;

bool NoBlood()
{
	if (!g_no_blood)
		g_no_blood = gi.cvar("g_no_blood", "0", CVAR_NOFLAGS); // 1: sparks for blood, no flesh gibs
	return g_no_blood->integer != 0;
}
} // namespace

int Core_Gore_HitEffect(int type)
{
	const bool blood = type == TE_BLOOD || type == TE_MOREBLOOD || type == TE_GREENBLOOD;
	return Core_Gore_HitToSparks(NoBlood(), blood) ? TE_SPARKS : type;
}

void Core_Gore_Gib(edict_t *gib, int32_t gib_type)
{
	switch (Core_Gore_GibLook(NoBlood(), gib_type & GIB_METALLIC, gib_type & GIB_DEBRIS))
	{
	case GibLook::Stock:
		break;
	case GibLook::NoTrail:
		gib->s.effects &= ~(EF_GIB | EF_GREENGIB);
		break;
	case GibLook::Hidden:
		gib->s.modelindex = 0;
		gib->s.modelindex2 = 0;
		gib->s.effects &= ~(EF_GIB | EF_GREENGIB);
		break;
	}
}
