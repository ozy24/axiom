// Jump's pure logic (jump.h).
#include "gametypes/jump/jump.h"

#include <cstdio>

bool Jump_Hurts(bool attacker_is_player, bool fall)
{
	return !attacker_is_player && !fall;
}

std::string Jump_TimeText(int64_t ms)
{
	if (ms < 0)
		ms = 0;
	char text[32];
	std::snprintf(text, sizeof(text), "%lld.%03lld s", static_cast<long long>(ms / 1000),
	              static_cast<long long>(ms % 1000));
	return text;
}
