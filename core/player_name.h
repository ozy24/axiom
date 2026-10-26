// A client's name for logs and records.
//
// Under the KEX lobby a human's pers.netname is a token, "##P<n>", that the
// client decodes when it arrives as a whole print argument (G_EncodedPlayerName).
// In-game prints pass netname as its own argument; text the server keeps
// (console lines, match records) needs the real name from userinfo.
#pragma once

#include <string>

struct edict_t;

std::string Core_PlayerName(const edict_t *ent);
