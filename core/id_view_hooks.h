// One centred name just above the match status (docs/design/hud.md): the
// player a spectator is chasing, else in team modes the player under the
// crosshair. Replaces stock's left-aligned stat_pname at yb -58 (team modes)
// and its CHASING / SPECTATOR MODE lines (deathmatch), which sat on top of
// the match status.
#pragma once

struct edict_t;
struct statusbar_t;

// From G_InitStatusbar's team-mode and deathmatch blocks, in place of stock's
// ID view and spectator lines.
void Core_IdView_Statusbar(statusbar_t &sb);

// From G_SetStats, after SetCTFStats has worked out STAT_CTF_ID_VIEW.
void Core_IdView_SetStats(edict_t *ent);

// From the end of G_SetSpectatorStats, after a chaser's stats were copied
// from its target: the chased player's name.
void Core_IdView_SetSpectatorStats(edict_t *ent);
