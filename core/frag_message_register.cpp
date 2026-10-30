// Engine-facing part of the frag messages (core/frag_message.h): who to tell,
// and the place among the players for FFA and Duel.
#include "g_local.h"

#include "core/frag_message.h"
#include "core/frag_message_hooks.h"
#include "core/gametype.h"
#include "core/match_hooks.h"

namespace
{
cvar_t *g_frag_messages;
} // namespace

void Core_FragMessage(edict_t *self, edict_t *attacker)
{
	if (!g_frag_messages)
		g_frag_messages = gi.cvar("g_frag_messages", "0", CVAR_NOFLAGS); // 1: the centre prints (built-in rulesets)
	if (!g_frag_messages->integer || !deathmatch->integer || !Core_Gametype_Active() || !attacker || attacker == self ||
	    !self->client || !attacker->client)
		return;
	if (!(self->svflags & SVF_BOT))
		gi.LocClient_Print(self, PRINT_CENTER, Core_FragVictimText().c_str(), attacker->client->pers.netname);
	if (attacker->svflags & SVF_BOT)
		return;
	const bool teams = G_TeamplayEnabled();
	std::vector<int> scores;
	for (uint32_t i = 0; i < game.maxclients; ++i)
	{
		const edict_t *ent = g_edicts + 1 + i;
		if (ent->inuse && ent->client && Core_Match_Playing(ent))
			scores.push_back(ent->client->resp.score);
	}
	const int score = attacker->client->resp.score;
	const std::string text = Core_FragKillerText(teams && OnSameTeam(self, attacker), !teams && Core_Match_InPlay(),
	                                             Core_FragPlace(scores, score), score);
	gi.LocClient_Print(attacker, PRINT_CENTER, text.c_str(), self->client->pers.netname);
}
