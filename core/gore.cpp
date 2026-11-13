// g_no_blood's decisions (core/gore.h).
#include "core/gore.h"

bool Core_Gore_HitToSparks(bool no_blood, bool blood_effect)
{
	return no_blood && blood_effect;
}

GibLook Core_Gore_GibLook(bool no_blood, bool metallic, bool debris)
{
	if (!no_blood || debris)
		return GibLook::Stock;
	return metallic ? GibLook::NoTrail : GibLook::Hidden;
}
