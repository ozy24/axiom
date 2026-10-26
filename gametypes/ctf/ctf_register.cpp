// Engine-facing part of CTF: the capturelimit, which stock checks in
// CTFCheckRules - not run while the match flow owns the rules.
#include "g_local.h"

const char *Ctf_MatchOver()
{
	if (!capturelimit->integer)
		return nullptr;
	const std::array<int, 2> caps = G_TeamCaptures();
	if (caps[0] < capturelimit->integer && caps[1] < capturelimit->integer)
		return nullptr;
	gi.LocBroadcast_Print(PRINT_HIGH, "$g_capturelimit_hit");
	return "capturelimit";
}
