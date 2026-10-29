// Capture The Flag: stock CTF (Core_Gametype_PreInit sets ctf 1), so id's CTF
// code runs unchanged. The match flow scores it by captures and ends it at the
// capturelimit (ctf_register.cpp).
#include "core/gametype.h"

const char *Ctf_MatchOver();

extern const GametypeDef gametype_ctf = {
    "ctf", "Capture The Flag", "ctf", nullptr, nullptr, Ctf_MatchOver, nullptr, nullptr, nullptr};
