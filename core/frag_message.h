// The centre prints when one player frags another (MuffMode's and Quake
// Live's; docs/design/hud.md), as pure logic. The adapter is
// frag_message_register.cpp.
//
//   the killer:  You fragged Bob                  team modes, warmup
//                You fragged Bob, your teammate :(
//                You fragged Bob                  FFA and Duel in play
//                2nd place with 12
//   the victim:  You were fragged by Alice
//
// Each text has a "{}" where the other player's name goes: a KEX name is a
// token the client decodes only as a whole print argument.
#pragma once

#include <string>
#include <vector>

struct FragPlace
{
	int place = 1;
	bool tied = false;
};

// `score`'s place among `scores` (everyone playing, the scorer included):
// one more than how many are higher; tied when someone else has the same.
FragPlace Core_FragPlace(const std::vector<int> &scores, int score);

// "1st", "2nd", "3rd", "11th", "22nd"; "Tied for 1st".
std::string Core_FragPlaceText(FragPlace place);

// The killer's text. `ranked`: FFA or Duel in play, so the second line says
// where the frag leaves them.
std::string Core_FragKillerText(bool teammate, bool ranked, FragPlace place, int score);

// The victim's text.
std::string Core_FragVictimText();
