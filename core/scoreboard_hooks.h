// The column scoreboard's hooks into id's code (docs/design/scoreboard.md),
// each a marked one-line call. The rules are core/scoreboard.h.
#pragma once

#include <string>

struct edict_t;

// From DeathmatchScoreboardMessage, before stock's rows: with g_scoreboard 1
// outside team modes, under a registered gametype, appends the column board
// for `ent` to `layout` and returns true (stock then draws no rows, only its
// frag limit, clock and intermission line). Else false: stock's board.
bool Core_Scoreboard_Columns(const edict_t *ent, std::string &layout);

// From player_die, once per death: counts it while the match is in play.
void Core_Scoreboard_Died(const edict_t *self);

// Every server frame, from Core_Match_Frame: time played, and a fresh count
// as the match starts.
void Core_Scoreboard_Frame();
