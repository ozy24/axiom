// Adapter: a client's real name and key (core/player_name.h).
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

std::string Core_PlayerKey(const edict_t *ent)
{
	const int slot = (int)(ent - g_edicts) - 1;
	if (!(ent->svflags & SVF_BOT) && ent->client->pers.social_id[0])
		return std::string("id:") + ent->client->pers.social_id;
	return "slot:" + std::to_string(slot);
}
