// Engine-facing powerup sounds (core/powerup_sound_hooks.h).
#include "g_local.h"

#include "core/gametype.h"
#include "core/powerup_sound_hooks.h"

void Core_Powerup_Sound(edict_t *ent, const char *sound)
{
	if (deathmatch->integer && Core_Gametype_Active())
	{
		// No attenuation and no PHS culling: the whole map, through walls.
		gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(sound), 1, ATTN_NONE, 0);
		return;
	}
	gi.sound(ent, CHAN_ITEM, gi.soundindex(sound), 1, ATTN_NORM, 0); // stock's
}
