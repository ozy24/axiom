// The frag messages' hook into id's code (core/frag_message.h), a marked
// one-line call.
#pragma once

struct edict_t;

// From player_die, after ClientObituary has scored the death: with
// g_frag_messages 1 in deathmatch, one player killing another tells the
// killer and the victim (humans only) in a centre print.
void Core_FragMessage(edict_t *self, edict_t *attacker);
