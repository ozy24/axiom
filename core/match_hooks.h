// The match flow's hooks into id's code (each a marked one-line call).
#pragma once

struct edict_t;

// Every server frame, from G_RunFrame, before the rules check.
void Core_Match_Frame();

// Whether the match flow, not stock CheckDMRules, decides when the level ends.
bool Core_Match_OwnsRules();

// `ready` / `notready`; true if handled. From ClientCommand, before id's chain.
bool Core_Match_ClientCommand(edict_t *ent, const char *cmd);
