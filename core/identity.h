// The mod's identity at run time: version string, serverinfo, banner.
// The values come from mod.json through the generated mod_identity.h.
#pragma once

#include <string>

// "<name> <version> (<build>)", e.g. "Name 0.1.0 (1a2b3c4d)".
std::string Core_VersionString();

// Registers the identity cvars (serverinfo gamename = mod id, g_version) and
// prints the banner. Called from InitGame in place of id's gamename line.
void Core_Identity_Register();
