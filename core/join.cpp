// Pure part of arriving players (core/join.h). No engine access.
#include "core/join.h"

#include <algorithm>
#include <cstdio>

std::string Core_Join_Hash(std::string_view key)
{
	uint32_t h = 2166136261u;
	for (const char c : key)
	{
		h ^= (unsigned char)c;
		h *= 16777619u;
	}
	char out[9];
	snprintf(out, sizeof(out), "%08x", h);
	return out;
}

void JoinMemory::Set(const std::vector<std::pair<std::string, int>> &playing)
{
	teams_.clear();
	for (const auto &[key, team] : playing)
		teams_[Core_Join_Hash(key)] = team;
}

std::string JoinMemory::Encode() const
{
	// Sorted, so the same players always give the same text (the adapter
	// writes the cvar only when it changes).
	std::vector<std::pair<std::string, int>> sorted(teams_.begin(), teams_.end());
	std::sort(sorted.begin(), sorted.end());
	std::string out;
	for (const auto &[hash, team] : sorted)
	{
		if (!out.empty())
			out += ' ';
		out += hash + ':' + std::to_string(team);
	}
	return out;
}

JoinMemory JoinMemory::Decode(std::string_view text)
{
	JoinMemory m;
	size_t at = 0;
	while (at < text.size())
	{
		size_t end = text.find(' ', at);
		if (end == std::string_view::npos)
			end = text.size();
		const std::string_view word = text.substr(at, end - at);
		at = end + 1;
		// "<8 hex>:<0-2>"
		if (word.size() != 10 || word[8] != ':' || word[9] < '0' || word[9] > '2')
			continue;
		bool hex = true;
		for (size_t i = 0; i < 8; ++i)
			hex = hex && ((word[i] >= '0' && word[i] <= '9') || (word[i] >= 'a' && word[i] <= 'f'));
		if (hex)
			m.teams_[std::string(word.substr(0, 8))] = word[9] - '0';
	}
	return m;
}

bool JoinMemory::Recall(const std::string &key, int &team) const
{
	const auto it = teams_.find(Core_Join_Hash(key));
	if (it == teams_.end())
		return false;
	team = it->second;
	return true;
}

void JoinMemory::KeepAbsent(const JoinMemory &last, const std::vector<std::string> &present)
{
	std::vector<std::string> here;
	for (const std::string &key : present)
		here.push_back(Core_Join_Hash(key));
	for (const auto &[hash, team] : last.teams_)
	{
		if (std::find(here.begin(), here.end(), hash) == here.end())
			teams_.emplace(hash, team);
	}
}

JoinArrival Core_Join_Arrive(const JoinWho &who, bool auto_join)
{
	if (who.remembered)
		return {true, who.team};
	if (who.bot || who.host || auto_join)
		return {true, 0};
	return {false, 0};
}
