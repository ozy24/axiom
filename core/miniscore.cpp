// Pure part of the miniscore (docs/design/hud.md). No engine access.
#include "core/miniscore.h"

#include <algorithm>
#include <cctype>

namespace
{
bool SkinChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

// Better placed: the higher score, then the lower slot.
bool Ahead(const MatchPlayer &a, const MatchPlayer &b)
{
	return a.score != b.score ? a.score > b.score : a.id < b.id;
}
} // namespace

std::array<int, 2> Core_Miniscore_Rows(const std::vector<MatchPlayer> &players)
{
	std::vector<MatchPlayer> playing;
	for (const MatchPlayer &p : players)
		if (p.playing)
			playing.push_back(p);
	std::sort(playing.begin(), playing.end(), Ahead);
	return {playing.size() > 0 ? playing[0].id : -1, playing.size() > 1 ? playing[1].id : -1};
}

std::string Core_Miniscore_IconPath(std::string_view skin)
{
	std::string s(skin);
	for (char &c : s)
		c = (char)std::tolower((unsigned char)c);
	const size_t slash = s.find('/');
	if (s.size() > 64 || slash == std::string::npos || slash == 0 || slash + 1 == s.size() ||
	    s.find('/', slash + 1) != std::string::npos)
		return {};
	if (!std::all_of(s.begin(), s.end(), [](char c) { return c == '/' || SkinChar(c); }))
		return {};
	return "/players/" + s + "_i";
}

bool MiniscoreIcons::Admit(const std::string &path)
{
	if (std::find(admitted_.begin(), admitted_.end(), path) != admitted_.end())
		return true;
	if (admitted_.size() >= limit_)
		return false;
	admitted_.push_back(path);
	return true;
}
