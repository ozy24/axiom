// Pure part of the rulesets: the built-ins and name handling. No engine access.
#include "core/rulesets.h"

const RulesetDef core_rulesets[] = {
    {"stock", "stock deathmatch: the match flow off, every mod setting at its default",
     "set g_gametype ffa\n"
     "set ctf 0\n"
     "set teamplay 0\n"
     "set match_warmup 0\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime 0\n"
     "set match_rounds 0\n"
     "set match_round_countdown 5\n"
     "set match_round_pause 2\n"
     "set match_suicide_point 0\n",
     "Stock"},
    {"match", "free for all with warmup and ready-up, 10 minutes, sudden death on a tie",
     "set g_gametype ffa\n"
     "set ctf 0\n"
     "set teamplay 0\n"
     "set match_warmup 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime -1\n"
     "set match_rounds 0\n"
     "set match_round_countdown 5\n"
     "set match_round_pause 2\n"
     "set match_suicide_point 0\n"
     "set timelimit 10\n"
     "set fraglimit 0\n"},
    {"duel", "one on one: warmup and ready-up, 10 minutes, sudden death on a tie, winner stays on",
     "set g_gametype duel\n"
     "set ctf 0\n"
     "set teamplay 0\n"
     "set match_warmup 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime -1\n"
     "set match_rounds 0\n"
     "set match_round_countdown 5\n"
     "set match_round_pause 2\n"
     "set match_suicide_point 0\n"
     "set timelimit 10\n"
     "set fraglimit 0\n"},
    {"hoony", "HoonyMode duel: every frag ends a round and resets the arena; first to 5, sudden death on a tie",
     "set g_gametype duel\n"
     "set ctf 0\n"
     "set teamplay 0\n"
     "set match_warmup 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime -1\n"
     "set match_rounds 1\n"
     "set match_round_countdown 5\n"
     "set match_round_pause 2\n"
     "set match_suicide_point 1\n"
     "set timelimit 15\n"
     "set fraglimit 5\n",
     "HoonyMode"},
    {"tdm", "team deathmatch: warmup and ready-up, 20 minutes, sudden death on a tie",
     "set g_gametype tdm\n"
     "set ctf 0\n"
     "set teamplay 1\n"
     "set match_warmup 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime -1\n"
     "set match_rounds 0\n"
     "set match_round_countdown 5\n"
     "set match_round_pause 2\n"
     "set match_suicide_point 0\n"
     "set timelimit 20\n"
     "set fraglimit 0\n"},
    {"ctf", "capture the flag: warmup and ready-up, 20 minutes or 8 captures, sudden death on a tie",
     "set g_gametype ctf\n"
     "set ctf 1\n"
     "set teamplay 0\n"
     "set match_warmup 1\n"
     "set match_ready 1\n"
     "set match_minplayers 2\n"
     "set match_warmup_time 0\n"
     "set match_countdown 10\n"
     "set match_overtime -1\n"
     "set match_rounds 0\n"
     "set match_round_countdown 5\n"
     "set match_round_pause 2\n"
     "set match_suicide_point 0\n"
     "set timelimit 20\n"
     "set fraglimit 0\n"
     "set capturelimit 8\n"},
};
const size_t core_ruleset_count = sizeof(core_rulesets) / sizeof(core_rulesets[0]);

const RulesetDef *Core_Ruleset_Find(std::string_view name)
{
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (name == core_rulesets[i].name)
			return &core_rulesets[i];
	}
	return nullptr;
}

std::string Core_Ruleset_Gametype(const RulesetDef &def)
{
	const std::string_view cfg = def.cfg;
	const std::string_view key = "set g_gametype ";
	const size_t at = cfg.find(key);
	if (at == std::string_view::npos)
		return {};
	const size_t start = at + key.size();
	return std::string(cfg.substr(start, cfg.find('\n', start) - start));
}

const RulesetDef *Core_Ruleset_Standard(std::string_view gametype)
{
	for (size_t i = 0; i < core_ruleset_count; ++i)
	{
		if (!core_rulesets[i].variant && Core_Ruleset_Gametype(core_rulesets[i]) == gametype)
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

std::string Core_Ruleset_Commands(std::string_view name)
{
	if (!Core_Ruleset_ValidName(name))
		return {};
	std::string text;
	if (const RulesetDef *def = Core_Ruleset_Find(name))
		text = def->cfg;
	else
		text = "exec rulesets/" + std::string(name) + ".cfg\n";
	text += "set g_ruleset " + std::string(name) + "\n";
	return text;
}
