// The team modes' player ID (the name under the crosshair), drawn centred just
// above the match status instead of stock's left-aligned stat_pname at yb -58,
// which sat on top of it (docs/design/hud.md).
#pragma once

struct edict_t;
struct statusbar_t;

// From G_InitStatusbar's team-mode block, in place of stock's ID view lines.
void Core_IdView_Statusbar(statusbar_t &sb);

// From G_SetStats, after SetCTFStats has worked out STAT_CTF_ID_VIEW.
void Core_IdView_SetStats(edict_t *ent);
