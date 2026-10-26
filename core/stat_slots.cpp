// Pure part of the stat-slot allocator: no engine access.
#include "core/stat_slots.h"

StatSlots::StatSlots(int first, int end) : first_(first), end_(end), next_(first) {}

std::optional<int> StatSlots::Allocate(std::string_view owner, int count)
{
	if (count <= 0 || owner.empty())
		return std::nullopt;
	for (const Range &r : ranges_)
	{
		if (r.owner == owner)
			return r.count == count ? std::optional<int>(r.first) : std::nullopt;
	}
	if (next_ + count > end_)
		return std::nullopt;
	ranges_.push_back({std::string(owner), next_, count});
	next_ += count;
	return ranges_.back().first;
}

int StatSlots::Remaining() const
{
	return end_ - next_;
}

std::vector<std::string> StatSlots::Describe() const
{
	std::vector<std::string> out;
	for (const Range &r : ranges_)
		out.push_back(r.owner + ": " + std::to_string(r.first) + ".." + std::to_string(r.first + r.count - 1));
	return out;
}
