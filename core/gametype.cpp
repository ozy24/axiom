// Pure part of the gametype registry: lookups, no engine access.
#include "core/gametype.h"

const GametypeDef *Core_Gametype_Find(const GametypeDef *const *table, size_t count, std::string_view id)
{
	for (size_t i = 0; i < count; ++i)
	{
		if (table[i] && id == table[i]->id)
			return table[i];
	}
	return nullptr;
}
