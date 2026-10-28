// Pure part of the map rotation (docs/decisions/0014). No engine access.
#include "core/maps.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace
{
std::string Lower(std::string_view s)
{
	std::string out(s);
	for (char &c : out)
		c = (char)std::tolower((unsigned char)c);
	return out;
}

bool Contains(const std::vector<std::string> &list, std::string_view s)
{
	return std::find(list.begin(), list.end(), s) != list.end();
}

bool SafeMapName(const std::string &word)
{
	if (word.empty() || word.size() > 32 || word.front() == '/' || word.back() == '/' ||
	    word.find("//") != std::string::npos)
		return false;
	return std::all_of(
	    word.begin(), word.end(),
	    [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/'; });
}

// FNV-1a over the order: the same pass always reshuffles the same way.
uint32_t Seed(const std::vector<std::string> &order)
{
	uint32_t h = 2166136261u;
	for (const std::string &m : order)
	{
		for (char c : m)
			h = (h ^ (uint8_t)c) * 16777619u;
		h = (h ^ ' ') * 16777619u;
	}
	return h;
}
} // namespace

std::vector<std::string> Core_Maps_Words(std::string_view text)
{
	std::vector<std::string> out;
	size_t i = 0;
	while (i < text.size())
	{
		while (i < text.size() && std::isspace((unsigned char)text[i]))
			++i;
		const size_t start = i;
		while (i < text.size() && !std::isspace((unsigned char)text[i]))
			++i;
		const std::string word = Lower(text.substr(start, i - start));
		if (SafeMapName(word) && !Contains(out, word))
			out.push_back(word);
	}
	return out;
}

std::vector<std::string> Core_Maps_Order(const std::vector<std::string> &list, const std::vector<std::string> &kept,
                                         std::string_view current, bool shuffle, std::mt19937 &rng)
{
	if (!shuffle)
		return list;
	if (kept.size() == list.size() && std::is_permutation(kept.begin(), kept.end(), list.begin()))
		return kept;
	std::vector<std::string> order = list;
	std::shuffle(order.begin(), order.end(), rng);
	const auto at = std::find(order.begin(), order.end(), Lower(current));
	if (at != order.end())
		std::rotate(order.begin(), at, at + 1); // the current map first, the rest still shuffled
	return order;
}

std::vector<std::string> Core_Maps_Reshuffle(const std::vector<std::string> &last, std::mt19937 &rng)
{
	// A map at position i of the last pass has n-1-i maps after it there;
	// at position j of the next, j more before its turn. It needs at least
	// n/2 between its plays, so it may not sit before n/2 - (n-1-i). Placing
	// one position at a time from the maps allowed there always succeeds:
	// the held-back maps are never more than the positions still to fill.
	const size_t n = last.size();
	const size_t gap = n / 2;
	std::vector<size_t> left(n);
	for (size_t i = 0; i < n; ++i)
		left[i] = i;
	const auto earliest = [&](size_t i) { return gap > n - 1 - i ? gap - (n - 1 - i) : 0; };

	std::vector<std::string> out;
	for (size_t pos = 0; pos < n; ++pos)
	{
		std::vector<size_t> allowed;
		for (size_t i : left)
			if (earliest(i) <= pos)
				allowed.push_back(i);
		const size_t pick = allowed[std::uniform_int_distribution<size_t>(0, allowed.size() - 1)(rng)];
		out.push_back(last[pick]);
		left.erase(std::find(left.begin(), left.end(), pick));
	}
	return out;
}

MapsNext Core_Maps_Next(const std::vector<std::string> &order, std::string_view current, bool shuffle)
{
	if (order.empty())
		return {};
	const auto at = std::find(order.begin(), order.end(), Lower(current));
	if (at == order.end())
		return {order.front(), order};
	if (at + 1 != order.end())
		return {*(at + 1), order};
	if (!shuffle)
		return {order.front(), order};
	std::mt19937 rng(Seed(order));
	std::vector<std::string> next = Core_Maps_Reshuffle(order, rng);
	return {next.front(), next};
}
