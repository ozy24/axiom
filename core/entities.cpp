// Map entities beyond stock: pure logic (core/entities.h).
#include "core/entities.h"

#include <cctype>
#include <cmath>

namespace
{
bool Same(std::string_view a, std::string_view b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
			return false;
	return true;
}

// The words of a list key: "ffa duel", "ffa,duel".
std::vector<std::string_view> Words(std::string_view list)
{
	std::vector<std::string_view> words;
	size_t i = 0;
	while (i < list.size())
	{
		while (i < list.size() && (list[i] == ' ' || list[i] == ',' || list[i] == '\t'))
			++i;
		const size_t start = i;
		while (i < list.size() && list[i] != ' ' && list[i] != ',' && list[i] != '\t')
			++i;
		if (i > start)
			words.push_back(list.substr(start, i - start));
	}
	return words;
}

// MuffMode's gametype names, as the mod's ids.
std::string_view GametypeId(std::string_view word)
{
	if (Same(word, "tournament"))
		return "duel";
	if (Same(word, "team"))
		return "tdm";
	return word;
}

bool HasGametype(std::string_view list, const EntitySpawnMode &mode)
{
	for (std::string_view word : Words(list))
		if (Same(GametypeId(word), mode.gametype))
			return true;
	return false;
}

// MuffMode's rulesets that play as Quake II (its notq2 means "not q2re or
// mm"); the mod plays as those.
bool HasQuake2Ruleset(std::string_view list)
{
	for (std::string_view word : Words(list))
		if (Same(word, "q2") || Same(word, "q2re") || Same(word, "mm"))
			return true;
	return false;
}

// A flag key: set unless empty or 0.
bool On(std::string_view value)
{
	return !value.empty() && value != "0";
}

constexpr std::string_view kKeys[] = {
    // which modes
    "gametype", "not_gametype", "notteam", "notfree", "powerups_on", "powerups_off", "no_powerups", "ruleset",
    "not_ruleset", "notq1", "notq2", "notq3", "notq3a", "notq4", "bfg_on", "bfg_off",
    // known, nothing to do here: Clan Arena's walls, bot and human spawns,
    // the map's credits, notes MuffMode's maps carry that it ignores too
    "notarena", "nobots", "nohumans", "author", "author2", "remasterer", "spawnpad", "arena", "noarena",
    // worldspawn: read by the adapter
    "no_dm_spawnpads", "no_dm_telepads"};

constexpr AmmoAlias kAmmo[] = {
    {"ammo_shells_small", "ammo_shells", 6, "models/vault/items/ammo/shells/small/tris.md2"},
    {"ammo_shells_large", "ammo_shells", 20, "models/vault/items/ammo/shells/large/tris.md2"},
    {"ammo_bullets_small", "ammo_bullets", 16, "models/vault/items/ammo/bullets/small/tris.md2"},
    {"ammo_bullets_large", "ammo_bullets", 100, "models/vault/items/ammo/bullets/large/tris.md2"},
    {"ammo_cells_small", "ammo_cells", 20, "models/vault/items/ammo/cells/small/tris.md2"},
    {"ammo_cells_large", "ammo_cells", 100, "models/vault/items/ammo/cells/large/tris.md2"},
    {"ammo_rockets_small", "ammo_rockets", 2, "models/vault/items/ammo/rockets/small/tris.md2"},
    {"ammo_slugs_small", "ammo_slugs", 3, "models/vault/items/ammo/slugs/small/tris.md2"},
    {"ammo_slugs_large", "ammo_slugs", 20, "models/vault/items/ammo/slugs/large/tris.md2"},
};

// The entity text's tokens, as the engine's COM_Parse reads them: braces,
// quoted strings, bare words; // comments skipped.
class Tokens
{
  public:
	explicit Tokens(std::string_view text) : text_(text) {}

	// The next token, or false at the end. `quoted`: it was in quotes.
	bool Next(std::string_view &token, bool &quoted)
	{
		for (;;)
		{
			while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])))
				++pos_;
			if (text_.compare(pos_, 2, "//") != 0)
				break;
			while (pos_ < text_.size() && text_[pos_] != '\n')
				++pos_;
		}
		if (pos_ >= text_.size())
			return false;
		quoted = text_[pos_] == '"';
		if (quoted)
		{
			const size_t start = ++pos_;
			while (pos_ < text_.size() && text_[pos_] != '"')
				++pos_;
			if (pos_ >= text_.size())
			{
				unterminated_ = true;
				return false;
			}
			token = text_.substr(start, pos_++ - start);
			return true;
		}
		const size_t start = pos_;
		if (text_[pos_] == '{' || text_[pos_] == '}')
			++pos_;
		else
			while (pos_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos_])) &&
			       text_[pos_] != '{' && text_[pos_] != '}' && text_[pos_] != '"')
				++pos_;
		token = text_.substr(start, pos_ - start);
		return true;
	}

	bool Unterminated() const
	{
		return unterminated_;
	}

  private:
	std::string_view text_;
	size_t pos_ = 0;
	bool unterminated_ = false;
};
} // namespace

bool Core_Entities_IsKey(std::string_view key)
{
	for (std::string_view known : kKeys)
		if (Same(key, known))
			return true;
	return false;
}

bool Core_Entities_Disabled(std::string_view list, std::string_view classname)
{
	const auto starts = [&](std::string_view prefix)
	{ return classname.size() > prefix.size() && Same(classname.substr(0, prefix.size()), prefix); };
	if (!(starts("item_") || starts("weapon_") || starts("ammo_")) || starts("item_flag_"))
		return false;
	for (std::string_view word : Words(list))
		if (Same(word, classname))
			return true;
	return false;
}

bool Core_Entities_Excluded(const EntityKeys &keys, const EntitySpawnMode &mode)
{
	for (const auto &[key, value] : keys)
	{
		if (Same(key, "gametype") && !HasGametype(value, mode))
			return true;
		if (Same(key, "not_gametype") && HasGametype(value, mode))
			return true;
		if (Same(key, "notteam") && On(value) && mode.teams)
			return true;
		if (Same(key, "notfree") && On(value) && !mode.teams)
			return true;
		if (Same(key, "powerups_on") && On(value) && !mode.powerups)
			return true;
		if ((Same(key, "powerups_off") || Same(key, "no_powerups")) && On(value) && mode.powerups)
			return true;
		if (Same(key, "ruleset") && !HasQuake2Ruleset(value))
			return true;
		if (Same(key, "not_ruleset") && HasQuake2Ruleset(value))
			return true;
		if (Same(key, "notq2") && On(value))
			return true;
		if (Same(key, "bfg_off") && On(value))
			return true;
	}
	return false;
}

const AmmoAlias *Core_Entities_AmmoAlias(std::string_view classname)
{
	for (const AmmoAlias &alias : kAmmo)
		if (Same(classname, alias.classname))
			return &alias;
	return nullptr;
}

bool Core_Entities_JumpVelocity(const EntityVec &from, const EntityVec &apex, float gravity, EntityVec &velocity)
{
	const float height = apex[2] - from[2];
	if (height <= 0 || gravity <= 0)
		return false;
	// Up for `time` seconds to the apex; across the same time.
	const float time = std::sqrt(height / (0.5f * gravity));
	velocity = {(apex[0] - from[0]) / time, (apex[1] - from[1]) / time, time * gravity};
	return true;
}

bool Core_Entities_ValidFile(std::string_view text, std::string &error)
{
	Tokens tokens(text);
	std::string_view token;
	bool quoted = false;
	size_t entities = 0;
	while (tokens.Next(token, quoted))
	{
		if (quoted || token != "{")
		{
			error = "expected { at entity " + std::to_string(entities + 1);
			return false;
		}
		bool closed = false;
		bool world = false;
		while (tokens.Next(token, quoted))
		{
			if (!quoted && token == "}")
			{
				closed = true;
				break;
			}
			const std::string_view key = token;
			if (!quoted && token == "{")
				break;
			if (!tokens.Next(token, quoted) || (!quoted && (token == "{" || token == "}")))
				break;
			if (entities == 0 && Same(key, "classname") && Same(token, "worldspawn"))
				world = true;
		}
		if (!closed)
		{
			error = tokens.Unterminated() ? "an unclosed quote"
			                              : "entity " + std::to_string(entities + 1) + " isn't closed";
			return false;
		}
		if (entities == 0 && !world)
		{
			error = "the first entity isn't worldspawn";
			return false;
		}
		++entities;
	}
	if (tokens.Unterminated())
	{
		error = "an unclosed quote";
		return false;
	}
	if (entities == 0)
	{
		error = "no entities";
		return false;
	}
	return true;
}
