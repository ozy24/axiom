// The match flow's hooks into id's code (each a marked one-line call).
#pragma once

struct edict_t;
struct gtime_t;

// Every server frame, from G_RunFrame, before the rules check.
void Core_Match_Frame();

// Whether the match flow, not stock CheckDMRules, decides when the level ends.
bool Core_Match_OwnsRules();

// The scoreboard clock: true and the level time it reaches zero, or false for
// no clock. Stock's timelimit from map load unless the match flow owns the
// rules; then none in warmup, the countdown or sudden death.
bool Core_Match_ClockEnd(gtime_t &end);

// `sv match`: the state, every client's score and the team scores; true if
// handled. From ServerCommand.
bool Core_Match_ServerCommand(const char *cmd);

// `ready` / `notready`; true if handled. From ClientCommand, before id's chain.
bool Core_Match_ClientCommand(edict_t *ent, const char *cmd);
