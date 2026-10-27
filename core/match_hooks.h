// The match flow's hooks into id's code (each a marked one-line call).
#pragma once

struct edict_t;
struct gtime_t;
struct statusbar_t;

// Every server frame, from G_RunFrame, before the rules check.
void Core_Match_Frame();

// Whether the match flow, not stock CheckDMRules, decides when the level ends.
bool Core_Match_OwnsRules();

// Whether a client is held in place for the round countdown (HoonyMode):
// from ClientThink, which then neither moves it nor lets it fire.
bool Core_Match_Frozen(const edict_t *ent);

// A suicide (or a death to the world) with match_suicide_point 1 and exactly
// one opponent, outside team modes: the point goes to the opponent instead of
// off the player (HoonyMode). True if it did; false, and stock scoring runs.
bool Core_Match_SuicidePoint(edict_t *self);

// The HUD's match-state line (docs/design/hud.md). Statusbar: from
// G_InitStatusbar, adds the line to the deathmatch status bar. SetStats: from
// G_SetStats, points the player's stat at the line's text while it shows.
void Core_Match_Statusbar(statusbar_t &sb);
void Core_Match_SetStats(edict_t *ent);

// The scoreboard clock: true and the level time it reaches zero, or false for
// no clock. Stock's timelimit from map load unless the match flow owns the
// rules; then none in warmup, the countdown or sudden death.
bool Core_Match_ClockEnd(gtime_t &end);

// `sv match`: the state, every client's score and the team scores; true if
// handled. From ServerCommand.
bool Core_Match_ServerCommand(const char *cmd);

// `ready` / `notready`; true if handled. From ClientCommand, before id's chain.
bool Core_Match_ClientCommand(edict_t *ent, const char *cmd);
