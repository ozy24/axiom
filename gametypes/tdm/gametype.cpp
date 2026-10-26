// Team Deathmatch: stock teamplay (Core_Gametype_PreInit sets teamplay 1), so
// id's team code runs unchanged. The match flow scores it by team frags.
#include "core/gametype.h"

extern const GametypeDef gametype_tdm = {"tdm",   "Team Deathmatch", "teamplay", nullptr,
                                         nullptr, nullptr,           nullptr,    nullptr};
