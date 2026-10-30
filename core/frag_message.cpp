// Pure part of the frag messages (core/frag_message.h). No engine access.
#include "core/frag_message.h"

#include <algorithm>

FragPlace Core_FragPlace(const std::vector<int> &scores, int score)
{
	FragPlace p;
	p.place = 1 + (int)std::count_if(scores.begin(), scores.end(), [&](int s) { return s > score; });
	p.tied = std::count(scores.begin(), scores.end(), score) > 1;
	return p;
}

std::string Core_FragPlaceText(FragPlace place)
{
	const int n = place.place;
	const int teens = n % 100;
	const char *suffix = "th";
	if (teens < 11 || teens > 13)
	{
		if (n % 10 == 1)
			suffix = "st";
		else if (n % 10 == 2)
			suffix = "nd";
		else if (n % 10 == 3)
			suffix = "rd";
	}
	return (place.tied ? "Tied for " : "") + std::to_string(n) + suffix;
}

std::string Core_FragKillerText(bool teammate, bool ranked, FragPlace place, int score)
{
	if (teammate)
		return "You fragged {}, your teammate";
	if (!ranked)
		return "You fragged {}";
	return "You fragged {}\n" + Core_FragPlaceText(place) + " place with " + std::to_string(score);
}

std::string Core_FragVictimText()
{
	return "You were fragged by {}";
}
