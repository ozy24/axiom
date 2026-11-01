// Match timeouts' hooks into id's code and the other core adapters
// (core/timeout.h, docs/design/timeouts.md).
#pragma once

struct edict_t;
struct usercmd_t;

// From G_RunFrame_, before level time advances: true while a timeout holds
// the world still, and the frame stops there (the HUD, stats and menus are
// kept up to date). Counts the timeout down.
bool Core_Timeout_Frame();

// A timeout holds the world still (paused or counting back to play).
bool Core_Timeout_Paused();

// From ClientThink, after the intermission's freeze: while paused, nobody
// moves or fires (PM_FREEZE), though an open Tab menu still works. True if
// ClientThink must stop.
bool Core_Timeout_Think(edict_t *ent, const usercmd_t *ucmd);

// `time-out` / `time-in` (and `timeout` / `timein` where the client passes
// them on; retail's own `timeout` cvar catches that name in its console), and
// `kill` refused while paused. True if handled.
bool Core_Timeout_ClientCommand(edict_t *ent, const char *cmd);

// `sv timeout`, `sv timein`: the server's own, outside the allowance.
bool Core_Timeout_ServerCommand(const char *cmd);

// A new match (the arena reset: going LIVE, back to warmup): every side has
// its timeout again; one under way is dropped.
void Core_Timeout_NewMatch();

// The HUD: the status line ("TIMEOUT 1:45", "RESUMING"; empty when none)
// and the resume countdown's big digits (0 when none).
const char *Core_Timeout_Status();
int Core_Timeout_CountdownSeconds();

// The Tab menu: whether this client may call one now, or end the one under way.
void Core_Timeout_MenuInfo(const edict_t *ent, bool &can_call, bool &can_end);

// `sv match`: the timeout under way, if any.
void Core_Timeout_PrintStatus();
