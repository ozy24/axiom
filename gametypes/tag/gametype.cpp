// Tag, the stock Ground Zero mode. Runs as Rogue gamerules RDM_TAG (2), so
// Rogue's own InitGameRules installs its Tag_* hooks, and the 22 places in
// Rogue's code that test gamerules keep working unchanged.
#include "core/gametype.h"

extern const GametypeDef gametype_tag = {"tag", "Tag", 2, nullptr};
