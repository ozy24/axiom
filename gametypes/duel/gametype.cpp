// Duel: one on one. The play queue decides who's in; everyone else watches.
// Engine-facing hooks are in duel_register.cpp; the queue is duel_queue.cpp.
#include "core/gametype.h"

struct dm_game_rt;
struct edict_t;
void Duel_Install(dm_game_rt &hooks);
void Duel_Frame();
const char *Duel_MatchOver();
void Duel_MatchEnded();
bool Duel_ClientCommand(edict_t *ent, const char *cmd);
bool Duel_Waiting(const edict_t *ent);
bool Duel_Scoreboard(const edict_t *viewer, size_t budget, std::string &layout);

extern const GametypeDef gametype_duel = {"duel",       "Duel",         "deathmatch",    Duel_Install,
                                          Duel_Frame,   Duel_MatchOver, Duel_MatchEnded, Duel_ClientCommand,
                                          Duel_Waiting, Duel_Scoreboard};
