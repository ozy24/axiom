// Pure part of the location callouts (core/loc.h). No engine access.
#include "core/loc.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <numeric>

namespace
{
constexpr float kFileScale = 8.0f; // .loc coordinates are eight times the map's

bool Space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

std::string_view Trim(std::string_view s)
{
	while (!s.empty() && Space(s.front()))
		s.remove_prefix(1);
	while (!s.empty() && Space(s.back()))
		s.remove_suffix(1);
	return s;
}

// The next word of `s`, which is advanced past it.
std::string_view Word(std::string_view &s)
{
	size_t i = 0;
	while (i < s.size() && Space(s[i]))
		++i;
	size_t j = i;
	while (j < s.size() && !Space(s[j]))
		++j;
	std::string_view word = s.substr(i, j - i);
	s.remove_prefix(j);
	return word;
}

bool Number(std::string_view word, float &out)
{
	if (word.empty() || word.size() > 31)
		return false;
	char buf[32];
	word.copy(buf, word.size());
	buf[word.size()] = '\0';
	char *end = nullptr;
	const float v = std::strtof(buf, &end);
	if (end != buf + word.size() || !std::isfinite(v))
		return false;
	out = v;
	return true;
}

float Distance2(LocVec a, LocVec b)
{
	const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

struct ItemName
{
	const char *classname;
	const char *name;
};

// MuffMode's short names, keyed by classname.
constexpr ItemName kItemNames[] = {
    {"weapon_shotgun", "SG"},
    {"weapon_supershotgun", "SSG"},
    {"weapon_machinegun", "MG"},
    {"weapon_chaingun", "CG"},
    {"weapon_grenadelauncher", "GL"},
    {"weapon_proxlauncher", "PROX"},
    {"weapon_rocketlauncher", "RL"},
    {"weapon_hyperblaster", "HB"},
    {"weapon_boomer", "RIPPER"},
    {"weapon_plasmabeam", "PB"},
    {"weapon_railgun", "RG"},
    {"weapon_phalanx", "PHALANX"},
    {"weapon_etf_rifle", "ETF"},
    {"weapon_disintegrator", "DISRUPTOR"},
    {"weapon_bfg", "BFG"},
    {"item_quad", "QUAD"},
    {"item_double", "DD"},
    {"item_quadfire", "DUALFIRE"},
    {"item_invulnerability", "INVUL"},
    {"item_invisibility", "INVIS"},
    {"item_enviro", "ENVIRO"},
    {"item_breather", "REBREATHER"},
    {"item_silencer", "SILENCER"},
    {"item_health_mega", "MEGA"},
};
} // namespace

std::string Core_Loc_CleanLabel(std::string_view label)
{
	std::string out;
	bool gap = false;
	for (const char ch : label)
	{
		const unsigned char c = static_cast<unsigned char>(ch);
		if (c < ' ' || c == 0x7f || Space(ch))
		{
			gap = !out.empty();
			continue;
		}
		if (gap)
		{
			if (out.size() + 1 >= kLocMaxLabel)
				break;
			out += ' ';
			gap = false;
		}
		if (out.size() >= kLocMaxLabel)
			break;
		out += ch;
	}
	return out;
}

bool Core_Loc_ParseLine(std::string_view line, LocPlace &out)
{
	float v[3];
	for (float &f : v)
	{
		if (!Number(Word(line), f))
			return false;
	}
	std::string label = Core_Loc_CleanLabel(Trim(line));
	if (label.empty())
		return false;
	out.origin = {v[0] / kFileScale, v[1] / kFileScale, v[2] / kFileScale};
	out.label = std::move(label);
	return true;
}

std::vector<LocPlace> Core_Loc_ParseFile(std::string_view text, size_t &skipped)
{
	std::vector<LocPlace> places;
	skipped = 0;
	while (!text.empty() && places.size() < kLocMaxPlaces)
	{
		const size_t end = text.find('\n');
		std::string_view line = Trim(text.substr(0, end));
		text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
		if (line.empty() || line.substr(0, 2) == "//" || line.front() == '#')
			continue;
		LocPlace place;
		if (Core_Loc_ParseLine(line, place))
			places.push_back(std::move(place));
		else
			++skipped;
	}
	return places;
}

int Core_Loc_Pick(const std::vector<LocPlace> &places, LocVec from, const std::function<bool(const LocPlace &)> &seen)
{
	if (places.empty())
		return -1;
	std::vector<size_t> order(places.size());
	std::iota(order.begin(), order.end(), size_t{0});
	const size_t tries = std::min(kLocVisibleTries, order.size());
	std::partial_sort(order.begin(), order.begin() + tries, order.end(), [&](size_t a, size_t b)
	                  { return Distance2(places[a].origin, from) < Distance2(places[b].origin, from); });
	for (size_t i = 0; i < tries; ++i)
	{
		if (seen && seen(places[order[i]]))
			return static_cast<int>(order[i]);
	}
	return static_cast<int>(order[0]);
}

std::string Core_Loc_ItemName(std::string_view classname)
{
	for (const ItemName &item : kItemNames)
	{
		if (classname == item.classname)
			return item.name;
	}
	const size_t cut = classname.find('_');
	std::string name(cut == std::string_view::npos ? classname : classname.substr(cut + 1));
	for (char &c : name)
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	return name.empty() ? std::string("?") : name;
}

std::string Core_Loc_ItemLabel(std::string_view name, float z, const std::vector<float> &twins)
{
	if (twins.empty())
		return std::string(name);
	const bool above_all = std::all_of(twins.begin(), twins.end(), [z](float t) { return t < z; });
	const bool below_all = std::all_of(twins.begin(), twins.end(), [z](float t) { return t > z; });
	if (above_all)
		return "upper " + std::string(name);
	if (below_all)
		return "lower " + std::string(name);
	return std::string(name);
}

std::string Core_Loc_ArmorText(int armor, int cells)
{
	std::string text = "A:" + std::to_string(armor);
	if (cells >= 0)
		text += " P:" + std::to_string(cells);
	return text;
}

std::string Core_Loc_Names(const std::vector<std::string> &names)
{
	if (names.empty())
		return "nobody";
	std::string out;
	const size_t shown = std::min(names.size(), kLocMaxNames);
	for (size_t i = 0; i < shown; ++i)
	{
		if (i)
			out += ", ";
		out += names[i];
	}
	if (names.size() > shown)
		out += " +" + std::to_string(names.size() - shown);
	return out;
}

bool Core_Loc_InView(LocVec eye, LocVec forward, LocVec target)
{
	LocVec d{target.x - eye.x, target.y - eye.y, target.z - eye.z};
	const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
	if (length <= 0.0f)
		return true;
	const float dot = (d.x * forward.x + d.y * forward.y + d.z * forward.z) / length;
	return dot > 0.3f; // stock's infront()
}

std::string Core_Loc_Text(std::string_view args)
{
	std::string_view text = Trim(args);
	if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
		text = Trim(text.substr(1, text.size() - 2));
	for (const char c : {'l', 'L', 'h', 'a', 'w', 'm', 'n', 'N'})
	{
		if (Core_Loc_Uses(text, c))
			return std::string(text);
	}
	return std::string();
}

bool Core_Loc_Uses(std::string_view text, char c)
{
	const char token[] = {'%', c, '\0'};
	return text.find(token) != std::string_view::npos;
}

std::string Core_Loc_Expand(std::string_view text, const LocValues &values)
{
	std::string out;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] != '%' || i + 1 == text.size())
		{
			out += text[i];
			continue;
		}
		const char c = text[i + 1];
		switch (c)
		{
		case 'l':
		case 'L':
			out += "[" + values.place + "]";
			break;
		case 'h':
			out += "H:" + std::to_string(values.health);
			break;
		case 'a':
			out += values.armor;
			break;
		case 'w':
			out += values.weapon;
			break;
		case 'm':
			out += values.ammo;
			break;
		case 'n':
			out += values.team;
			break;
		case 'N':
			out += values.all;
			break;
		default:
			out += '%';
			continue; // the next character is copied as typed
		}
		++i;
	}
	if (out.size() > kLocMaxText)
		out.resize(kLocMaxText);
	return out;
}
