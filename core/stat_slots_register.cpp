// Engine-facing part of the stat-slot allocator: stock's free range.
#include "g_local.h"

#include "core/stat_slots.h"

static_assert(STAT_LAST <= MAX_STATS, "stock stats overflow MAX_STATS");

StatSlots &Core_StatSlots()
{
	static StatSlots slots(STAT_LAST, MAX_STATS);
	return slots;
}
