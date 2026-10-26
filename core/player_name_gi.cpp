// Adapter: a client's real name (core/player_name.h).
#include "g_local.h"

#include "core/player_name.h"

std::string Core_PlayerName(const edict_t *ent)
{
	if (!ent || !ent->client)
		return {};
	char name[MAX_INFO_VALUE] = {};
	if (!(ent->svflags & SVF_BOT) && gi.Info_ValueForKey(ent->client->pers.userinfo, "name", name, sizeof(name)) &&
	    name[0])
		return name;
	return ent->client->pers.netname;
}
