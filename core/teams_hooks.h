// Team locks and captains' hooks into id's code (each a marked one-line call)
// and into the mod's own (docs/design/teams.md, decision 0016).
#pragma once

#include <string>

struct edict_t;
struct MenuView;

// Every server frame, from Core_Match_Frame: keeps the teams up to date,
// locks them as the match goes live (match_lock) and unlocks them for warmup.
void Core_Teams_Frame();

// Whether `ent` may join `team` (1 red, 2 blue) now; if not, and `tell`,
// says why to `ent` alone. From stock's team joins (CTFChangeTeam,
// CTFJoinTeam) and core/join_register.cpp's choice of team.
bool Core_Teams_MayJoin(edict_t *ent, int team, bool tell = true);

// `captain`, `lockteam`, `unlockteam`, `teamkick`, `readyteam`; true if
// handled. From ClientCommand, before id's chain.
bool Core_Teams_ClientCommand(edict_t *ent, const char *cmd);

// A passed `callvote shuffle`, or `sv shuffle`: before LIVE, splits the
// players on teams into two new teams at random (Core_Teams_Shuffle), unlocks
// both and respawns everyone on a team. False, and nothing changes, once the
// match is live or with the match flow off.
bool Core_Teams_ShuffleNow();

// `sv spectate` in a team game (core/join_register.cpp): `target` off their
// team and its roster, so a locked team keeps them out, to spectating. Bot
// and NoTeam: nothing done.
enum class TeamsResult;
TeamsResult Core_Teams_Remove(edict_t *target);

// `sv lockteam`, `sv unlockteam` (a team, or both), `sv teams`, `sv shuffle`;
// true if handled. From ServerCommand.
bool Core_Teams_ServerCommand(const char *cmd);

// The Tab menu: the locks and captains as `ent` sees them, into its view.
void Core_Teams_MenuInfo(const edict_t *ent, MenuView &view);

// A captain's command from the Tab menu, as the console command `cmd`
// (`lockteam`, ...) with `target` the client it acts on (teamkick, captain).
void Core_Teams_Run(edict_t *ent, const char *cmd, edict_t *target);

// The scoreboard's captain marker: a `*` left of a captain's row at (x, y),
// as layout text, else empty. From CTFScoreboardMessage, before the row.
std::string Core_Teams_ScoreboardMark(const edict_t *ent, int x, int y);
