// Pure part of Duel: the play queue. No engine access.
#include "gametypes/duel/duel_queue.h"

#include <algorithm>

void DuelQueue::Sync(const std::vector<std::string> &present, bool keep_absent)
{
	if (!keep_absent)
		order_.erase(std::remove_if(order_.begin(), order_.end(), [&](const std::string &id)
		                            { return std::find(present.begin(), present.end(), id) == present.end(); }),
		             order_.end());
	for (const std::string &id : present)
	{
		if (std::find(order_.begin(), order_.end(), id) == order_.end())
			order_.push_back(id);
	}
}

std::vector<std::string> DuelQueue::Players() const
{
	return std::vector<std::string>(order_.begin(), order_.begin() + std::min<size_t>(2, order_.size()));
}

bool DuelQueue::IsPlayer(const std::string &id) const
{
	const std::vector<std::string> players = Players();
	return std::find(players.begin(), players.end(), id) != players.end();
}

void DuelQueue::Rotate(const std::string &loser)
{
	auto it = std::find(order_.begin(), order_.end(), loser);
	if (it == order_.end())
		return;
	order_.erase(it);
	order_.push_back(loser);
}
