// The match flow's hooks into id's code (each a marked one-line call).
#pragma once

struct edict_t;
struct gtime_t;
struct statusbar_t;

// Every server frame, from G_RunFrame, before the rules check.
void Core_Match_Frame();

// Whether the match flow, not stock CheckDMRules, decides when the level ends.
bool Core_Match_OwnsRules();

// Whether a client may move but not fire: the match countdown. From
// ClientThink, which then drops the fire button. Never the dead, so they can
// press it to respawn.
bool Core_Match_HoldFire(const edict_t *ent);

// The HUD's match-state line (docs/design/hud.md). Statusbar: from
// G_InitStatusbar, adds the line to the deathmatch status bar. SetStats: from
// G_SetStats, points the player's stat at the line's text while it shows.
void Core_Match_Statusbar(statusbar_t &sb);
void Core_Match_SetStats(edict_t *ent);

// The scoreboard clock: true and the level time it reaches zero, or false for
// no clock. Stock's timelimit from map load unless the match flow owns the
// rules; then none in warmup, the countdown or sudden death.
bool Core_Match_ClockEnd(gtime_t &end);

// `sv match`: the state, every client's score and the team scores; `sv
// readyall`, `sv readyteam red|blue`; true if handled. From ServerCommand.
bool Core_Match_ServerCommand(const char *cmd);

// The Tab menu's Ready row (core/menu_register.cpp): whether ready-up is on
// now (warmup or the countdown, with match_ready), a client's state, and
// setting it as `ready` / `notready` do.
bool Core_Match_ReadyUp();
bool Core_Match_Warmup(); // the match flow is in warmup (votes are open there by default)
bool Core_Match_InPlay(); // being played: live or between rounds; with the flow off, until the intermission
// `sv spectate` took a duellist out of a match under way: back to warmup
// (Match::BackToWarmup). Nothing outside play or with the flow off.
void Core_Match_BackToWarmup();
// The timelimit or fraglimit cvar changed (a passed setting vote): the match
// under way takes the new limits without a restart (Match::ChangeLimits).
void Core_Match_LimitsChanged();
bool Core_Match_Playing(const edict_t *ent); // in the match: begun, not a spectator, on a team in team modes
bool Core_Match_IsReady(const edict_t *ent); // bots always are
// `announce` false: no messages at all (a captain's readyteam says it once).
void Core_Match_SetReady(edict_t *ent, bool ready, bool announce = true);

// `ready` / `notready`; true if handled. From ClientCommand, before id's chain.
bool Core_Match_ClientCommand(edict_t *ent, const char *cmd);
