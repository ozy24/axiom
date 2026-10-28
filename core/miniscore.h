// The miniscore outside team modes (docs/design/hud.md): two rows bottom
// right, each a player's face (their skin's icon) and score, while the match
// is live. Pure logic; the adapter is core/miniscore_register.cpp.
#pragma once

#include "core/match.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

// The two rows, top first: the two best placed playing players, by score
// then the lower slot; -1 for an empty row. The same for every viewer (your
// own score is stock's frag counter's).
std::array<int, 2> Core_Miniscore_Rows(const std::vector<MatchPlayer> &players);

// The image for a skin ("male/grunt"): "/players/male/grunt_i", the icon
// stock's scoreboard draws. Empty unless the skin is model/skin of
// [a-z0-9_-] (lowercased), so userinfo can't reach outside players/.
std::string Core_Miniscore_IconPath(std::string_view skin);

// Which icons get an image index this level. Each one takes one of the
// engine's 512 image configstrings until the level ends, and players can
// change skin at will, so at most `limit` distinct icons; later ones show no
// face. Without a cap, cycling skins could fill the table, and the next
// gi.imageindex is a fatal error.
class MiniscoreIcons
{
  public:
	explicit MiniscoreIcons(size_t limit) : limit_(limit) {}

	// True if `path` may be registered: already admitted, or room for it.
	bool Admit(const std::string &path);
	void Reset()
	{
		admitted_.clear();
	}

  private:
	size_t limit_;
	std::vector<std::string> admitted_;
};
