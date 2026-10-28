// The miniscore's hooks into id's HUD code (docs/design/hud.md), each a
// marked one-line call. The rules are core/miniscore.h.
#pragma once

struct edict_t;
struct statusbar_t;

// From G_InitStatusbar's deathmatch block: outside team modes, under a
// registered gametype, adds the two rows. It reuses stock's CTF team-score
// stats (STAT_CTF_TEAM1_PIC..TEAM2_CAPS), which nothing shows there. Runs
// once per level, so it also forgets the last level's face images.
void Core_Miniscore_Statusbar(statusbar_t &sb);

// From G_SetStats, after SetCTFStats has written its values into those
// stats: the top two players' faces and scores while the match is in play,
// else none.
void Core_Miniscore_SetStats(edict_t *ent);
