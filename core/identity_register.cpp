// Engine-facing part of the identity.
#include "g_local.h"

#include "core/host.h"
#include "core/identity.h"
#include "mod_identity.h"

void Core_Identity_Register()
{
	// id registered gamename as GAMEVERSION ("baseq2"). Neither engine reads it;
	// it identifies the mod to server browsers and query tools, by convention a
	// short lowercase id (docs/decisions/0008). GAMEVERSION itself stays: it is
	// also the base game folder (g_svcmds.cpp listip.cfg fallback).
	gi.cvar("gamename", MOD_ID, CVAR_SERVERINFO | CVAR_LATCH);

	gi.cvar("g_version", Core_VersionString().c_str(), CVAR_SERVERINFO | CVAR_NOSET);
	Core_Identity_PrintBanner(Core_GameHost());
}
