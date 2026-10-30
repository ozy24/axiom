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

// Whether a client may move but not fire: the match countdown. From
// ClientThink, which then drops the fire button. Never the dead, so they can
// press it to respawn.
bool Core_Match_HoldFire(const edict_t *ent);

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

// The Tab menu's Ready row (core/menu_register.cpp): whether ready-up is on
// now (warmup or the countdown, with match_ready), a client's state, and
// setting it as `ready` / `notready` do.
bool Core_Match_ReadyUp();
bool Core_Match_Warmup(); // the match flow is in warmup (votes are open there by default)
bool Core_Match_InPlay(); // being played: live or between rounds; with the flow off, until the intermission
// The timelimit or fraglimit cvar changed (a passed setting vote): the match
// under way takes the new limits without a restart (Match::ChangeLimits).
void Core_Match_LimitsChanged();
bool Core_Match_Playing(const edict_t *ent); // in the match: begun, not a spectator, on a team in team modes
bool Core_Match_IsReady(const edict_t *ent); // bots always are
// `announce` false: no messages at all (a captain's readyteam says it once).
void Core_Match_SetReady(edict_t *ent, bool ready, bool announce = true);

// Elimination rounds (Clan Arena, docs/design/clan-arena.md). Died: from
// player_die, a life gone (out of the round on the last). HoldOut: from
// PutClientInServer after CTFStartClient: true if `ent` spawned into a round
// it isn't in (a late joiner, a team switcher, the eliminated), and it was
// made a watcher instead. Watching: out of the round, following teammates.
// MayChase: whether `ent` may follow `target` (an eliminated player, only a
// living teammate). WatchNext: from ClientThink's spectator fire, true if it
// moved a watcher on to the next teammate (never free cam). HoldWatcher: a
// watcher with nobody to follow is frozen (from ClientThink's pm_type).
// Disconnect: their place in the round goes.
void Core_Match_Died(edict_t *self);
bool Core_Match_HoldOut(edict_t *ent);
bool Core_Match_Watching(const edict_t *ent);
bool Core_Match_MayChase(const edict_t *ent, const edict_t *target);
bool Core_Match_WatchNext(edict_t *ent);
bool Core_Match_HoldWatcher(const edict_t *ent);
void Core_Match_Disconnect(edict_t *ent);

// `ready` / `notready`; true if handled. From ClientCommand, before id's chain.
bool Core_Match_ClientCommand(edict_t *ent, const char *cmd);
