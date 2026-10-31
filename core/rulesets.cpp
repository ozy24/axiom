// Pure part of the rulesets: the built-ins, names and the switch's console
// text (docs/decisions/0013). No engine access.
#include "core/rulesets.h"

#include <algorithm>
#include <cctype>

const RulesetDef core_rulesets[] = {
    {"ffa", "ffa", "free for all with warmup and ready-up, 10 minutes, sudden death on a tie",
     "set g_gametype ffa\n"
     "set ctf 0\n"
     "set teamplay 0\n"
     "set match_flow 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_allow_no_humans 0\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime -1\n"
     "set match_overtime_max 30\n"
     "set match_lock 0\n"
     "set match_warmup_weapons 1\n"
     "set timelimit 10\n"
     "set fraglimit 0\n"
     "set g_map_list_shuffle 1\n"
     "set g_scoreboard 1\n"
     "set g_frag_messages 1\n"},
    {"duel", "duel", "one on one: warmup and ready-up, 10 minutes, 2-minute overtimes on a tie, winner stays on",
     "set g_gametype duel\n"
     "set ctf 0\n"
     "set teamplay 0\n"
     "set match_flow 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_allow_no_humans 0\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime 2\n"
     "set match_overtime_max 30\n"
     "set match_lock 0\n"
     "set match_warmup_weapons 1\n"
     "set timelimit 10\n"
     "set fraglimit 0\n"
     "set g_map_list_shuffle 1\n"
     "set g_scoreboard 1\n"
     "set g_frag_messages 1\n"},
    {"tdm", "tdm", "team deathmatch: warmup and ready-up, 20 minutes, 2-minute overtimes on a tie",
     "set g_gametype tdm\n"
     "set ctf 0\n"
     "set teamplay 1\n"
     "set match_flow 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_allow_no_humans 0\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime 2\n"
     "set match_overtime_max 30\n"
     "set match_lock 1\n"
     "set match_warmup_weapons 1\n"
     "set timelimit 20\n"
     "set fraglimit 0\n"
     "set g_map_list_shuffle 1\n"
     "set g_scoreboard 1\n"
     "set g_frag_messages 1\n"},
    {"ctf", "ctf", "capture the flag: warmup and ready-up, 20 minutes or 8 captures, 2-minute overtimes on a tie",
     "set g_gametype ctf\n"
     "set ctf 1\n"
     "set teamplay 0\n"
     "set match_flow 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_allow_no_humans 0\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime 2\n"
     "set match_overtime_max 30\n"
     "set match_lock 1\n"
     "set match_warmup_weapons 1\n"
     "set timelimit 20\n"
     "set fraglimit 0\n"
     "set g_map_list_shuffle 1\n"
     "set capturelimit 8\n"
     "set g_scoreboard 1\n"
     "set g_frag_messages 1\n"},
    // The four core gametypes only (decision 0022): no built-in variants. An
    // admin lists their own in g_ruleset_variants.
};
const size_t core_ruleset_count = sizeof(core_rulesets) / sizeof(core_rulesets[0]);

// The defaults their modules register: g_gametype (core/gametype_register.cpp),
// match_* (core/match_register.cpp), the limits (rerelease/g_main.cpp),
// g_map_list, g_map_list_shuffle, g_no_items and g_friendly_fire
// (rerelease/g_main.cpp).
const GameplayCvar core_gameplay_cvars[] = {
    {"g_gametype", "ffa"},
    {"ctf", "0"},
    {"teamplay", "0"},
    {"match_flow", "0"},
    {"match_ready", "1"},
    {"match_minplayers", "2"},
    {"match_allow_no_humans", "0"},
    {"match_warmup_time", "0"},
    {"match_countdown", "10"},
    {"match_overtime", "0"},
    {"match_overtime_max", "0"},
    {"match_lock", "0"},
    {"match_warmup_weapons", "1"},
    {"timelimit", "0"},
    {"fraglimit", "0"},
    {"capturelimit", "0"},
    {"g_map_list", ""},
    {"g_map_list_shuffle", "0"},
    {"g_no_items", "0"},      // the powerups vote (decision 0015)
    {"g_friendly_fire", "0"}, // the friendly fire vote
    {"g_scoreboard", "0"},    // the column board (core/scoreboard_register.cpp)
    {"g_frag_messages", "0"}, // the centre prints on a frag (core/frag_message_register.cpp)
};
const size_t core_gameplay_cvar_count = sizeof(core_gameplay_cvars) / sizeof(core_gameplay_cvars[0]);

const RulesetDef *Core_Ruleset_Find(std::string_view name)
{
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (name == core_rulesets[i].name)
			return &core_rulesets[i];
	}
	return nullptr;
}

bool Core_Ruleset_ValidName(std::string_view name)
{
	if (name.empty() || name.size() > 32)
		return false;
	for (char c : name)
	{
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return false;
	}
	return true;
}

namespace
{
const RulesetDef *Own(std::string_view gametype)
{
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (!core_rulesets[i].variant && gametype == core_rulesets[i].gametype)
			return &core_rulesets[i];
	}
	return nullptr;
}

std::vector<std::string_view> Words(std::string_view text)
{
	std::vector<std::string_view> out;
	size_t i = 0;
	while (i < text.size())
	{
		while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == ','))
			++i;
		const size_t start = i;
		while (i < text.size() && text[i] != ' ' && text[i] != '\t' && text[i] != ',')
			++i;
		if (i > start)
			out.push_back(text.substr(start, i - start));
	}
	return out;
}

// "rocket_arena" -> "Rocket Arena"
std::string Label(std::string_view words)
{
	std::string out;
	bool start = true;
	for (char c : words)
	{
		if (c == '_' || c == '-')
		{
			out += ' ';
			start = true;
			continue;
		}
		out += start && c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
		start = false;
	}
	return out;
}

// An admin's variant: listed in g_ruleset_variants, named <gametype>_<variant>.
std::optional<Ruleset> Custom(std::string_view name, std::string_view g_ruleset_variants,
                              const std::vector<std::string> &gametypes)
{
	if (!Core_Ruleset_ValidName(name) || Core_Ruleset_Find(name))
		return std::nullopt;
	bool listed = false;
	for (std::string_view w : Words(g_ruleset_variants))
		listed |= w == name;
	if (!listed)
		return std::nullopt;
	for (const std::string &gt : gametypes)
	{
		if (name.size() > gt.size() + 1 && name.substr(0, gt.size()) == gt && name[gt.size()] == '_')
			return Ruleset{std::string(name), gt, Label(name.substr(gt.size() + 1)), nullptr};
	}
	return std::nullopt;
}
} // namespace

std::optional<Ruleset> Core_Ruleset_Resolve(std::string_view name, std::string_view g_ruleset_variants,
                                            const std::vector<std::string> &gametypes)
{
	if (const RulesetDef *def = Core_Ruleset_Find(name))
		return Ruleset{def->name, def->gametype, def->variant ? def->variant : "", def};
	return Custom(name, g_ruleset_variants, gametypes);
}

std::vector<Ruleset> Core_Ruleset_All(std::string_view g_ruleset_variants, const std::vector<std::string> &gametypes)
{
	std::vector<Ruleset> out;
	for (const std::string &gt : gametypes)
	{
		if (const RulesetDef *own = Own(gt))
			out.push_back(*Core_Ruleset_Resolve(own->name, g_ruleset_variants, gametypes));
		for (size_t i = 0; i < core_ruleset_count; ++i)
			if (core_rulesets[i].variant && gt == core_rulesets[i].gametype)
				out.push_back(*Core_Ruleset_Resolve(core_rulesets[i].name, g_ruleset_variants, gametypes));
		for (std::string_view w : Words(g_ruleset_variants))
		{
			const std::optional<Ruleset> custom = Custom(w, g_ruleset_variants, gametypes);
			const bool dup = std::any_of(out.begin(), out.end(), [&](const Ruleset &r) { return r.name == w; });
			if (custom && custom->gametype == gt && !dup)
				out.push_back(*custom);
		}
	}
	return out;
}

std::string Core_Ruleset_Commands(const Ruleset &r)
{
	std::string text;
	for (size_t i = 0; i < core_gameplay_cvar_count; ++i)
		text += std::string("set ") + core_gameplay_cvars[i].name + " \"" + core_gameplay_cvars[i].value + "\"\n";
	if (const RulesetDef *own = Own(r.gametype))
		text += own->cfg;
	else
		text += "set g_gametype " + r.gametype + "\n";
	text += "exec rulesets/" + r.gametype + ".cfg\n";
	if (!r.variant.empty())
	{
		if (r.builtin)
			text += r.builtin->cfg;
		text += "exec rulesets/" + r.name + ".cfg\n";
	}
	text += "set g_ruleset " + r.name + "\n";
	return text;
}

namespace
{
// The console's tokens: quoted strings, or runs of non-space.
std::vector<std::string> Tokens(std::string_view line)
{
	std::vector<std::string> out;
	size_t i = 0;
	while (i < line.size())
	{
		while (i < line.size() && (unsigned char)line[i] <= ' ')
			++i;
		if (i >= line.size())
			break;
		if (line.compare(i, 2, "//") == 0)
			break;
		std::string token;
		if (line[i] == '"')
		{
			++i;
			while (i < line.size() && line[i] != '"')
				token += line[i++];
			++i;
		}
		else
		{
			while (i < line.size() && (unsigned char)line[i] > ' ')
				token += line[i++];
		}
		out.push_back(std::move(token));
	}
	return out;
}

bool Is(std::string_view word, std::string_view lower)
{
	return word.size() == lower.size() && std::equal(word.begin(), word.end(), lower.begin(), [](char a, char b)
	                                                 { return std::tolower((unsigned char)a) == b; });
}
} // namespace

std::vector<RulesetCfgStep> Core_Ruleset_ParseCfg(std::string_view text)
{
	std::vector<RulesetCfgStep> steps;
	std::string line;
	bool quoted = false;
	const auto flush = [&]
	{
		const std::vector<std::string> t = Tokens(line);
		line.clear();
		if (t.size() >= 3 && (Is(t[0], "set") || Is(t[0], "seta") || Is(t[0], "sets") || Is(t[0], "setu")))
			steps.push_back({false, t[1], t[2]});
		else if (t.size() >= 2 && Is(t[0], "exec"))
			steps.push_back({true, t[1], {}});
	};
	for (const char c : text)
	{
		if (c == '"')
			quoted = !quoted;
		if (c == '\n' || c == '\r' || (c == ';' && !quoted))
		{
			quoted = false;
			flush();
			continue;
		}
		line += c;
	}
	flush();
	return steps;
}
