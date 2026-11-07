// Jump: a downstream gametype. Its definition is plain data, found by the
// base's build through GAME_GAMETYPE_DIRS and registered from GAME_GAMETYPES.
#include "core/gametype.h"

void Jump_Install(dm_game_rt &hooks);
bool Jump_ClientCommand(edict_t *ent, const char *cmd);

// id, title, the stock mode it runs on (what retail's browser shows), then
// the hooks: install, frame, match_over, match_ended, client_command,
// waiting, scoreboard. Null means stock behaviour.
extern const GametypeDef gametype_jump = {"jump",  "Jump",  "deathmatch",       Jump_Install, nullptr,
                                          nullptr, nullptr, Jump_ClientCommand, nullptr,      nullptr};
