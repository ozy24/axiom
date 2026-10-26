// DeathBall, the stock Ground Zero mode. Runs as Rogue gamerules RDM_DEATHBALL
// (3); Rogue's InitGameRules installs its DBall_* hooks.
#include "core/gametype.h"

extern const GametypeDef gametype_ball = {"ball", "DeathBall", 3, nullptr};
