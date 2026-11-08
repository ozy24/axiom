// Deathmatch names are cut to 15 characters, classic Quake II's limit. A
// team board draws each name in full from its row's slot, so a long one ran
// from RED's column into BLUE's (a retail bot's, 2026-11-08). The cut
// counts UTF-8 characters and never splits one. Lobby players' names come
// from the platform, so the server can't shorten what their clients show.
#pragma once

#include <cstddef>
#include <string>

constexpr size_t kNameMaxChars = 15;

std::string Core_Name_Cap(const std::string &name, size_t max_chars = kNameMaxChars);
