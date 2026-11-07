// Adapter: a client's real name and key (core/player_name.h).
#include "g_local.h"

#include "core/player_name.h"

#include <cctype>

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

edict_t *Core_PlayerNamed(const char *arg)
{
	const auto present = [](const edict_t *ent) { return ent->inuse && ent->client && ent->client->pers.connected; };
	edict_t *found = nullptr;
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		edict_t *ent = g_edicts + i;
		if (present(ent) && !Q_strcasecmp(Core_PlayerName(ent).c_str(), arg))
		{
			if (found)
				return nullptr;
			found = ent;
		}
	}
	if (found)
		return found;
	bool digits = *arg != '\0';
	for (const char *c = arg; *c; ++c)
		digits = digits && isdigit((unsigned char)*c);
	if (digits)
	{
		const uint32_t n = (uint32_t)atoi(arg);
		if (n < game.maxclients && present(g_edicts + 1 + n))
			return g_edicts + 1 + n;
	}
	return nullptr;
}
