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

// The team board's spectators (TDM, CTF) under g_scoreboard 1, from
// CTFScoreboardMessage. Title: appends the Spectators heading at `y` and
// returns true, or false and stock writes its own. First: a spectator row's
// first number, 0 (stock: the score).
bool Core_Scoreboard_TeamSpectatorTitle(std::string &layout, int y);
int Core_Scoreboard_TeamSpectatorFirst(const edict_t *ent);

// The team board's ready marks (TDM, CTF), in ready-up: appended to stock's
// layout once its rows and spectators are in, within `max` bytes. From
// CTFScoreboardMessage, before it sends the layout.
void Core_Scoreboard_TeamReady(std::string &layout, size_t max);
