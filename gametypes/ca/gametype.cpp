// Clan Arena (decision 0021, docs/design/clan-arena.md): data only. Stock
// teamplay underneath (Core_Gametype_PreInit sets teamplay 1, as for TDM);
// every rule - elimination rounds, the arena loadout, no self damage, the
// damage score - is a match-flow setting its built-in ruleset `ca` turns on.
// Its own id gives it its own vote row and the Q2REX browser's "Clan Arena".
#include "core/gametype.h"

extern const GametypeDef gametype_ca = {"ca",    "Clan Arena", "teamplay", nullptr, nullptr,
                                        nullptr, nullptr,      nullptr,    nullptr, nullptr};
