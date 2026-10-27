// Rulesets: named profiles of settings, applied as console commands
// (decision 0003: .cfg text through the engine, no file access in the module).
//
//   sv ruleset <name>   apply a built-in ruleset, or exec rulesets/<name>.cfg
//   sv rulesets         list the built-ins
//
// Built-ins are compiled in, so the module works with no files at all. A
// server owner adds their own as rulesets/<name>.cfg (in the game folder or a
// pak); built-in names are reserved. Latched settings (g_gametype) take
// effect at the next map, so apply a ruleset before `map`.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct RulesetDef
{
	const char *name;
	const char *description;
	const char *cfg;       // console commands, one per line
	bool standard = false; // its gametype's standard settings (a gametype vote applies it)
};

extern const RulesetDef core_rulesets[];
extern const size_t core_ruleset_count;

// Built-in by exact name, or nullptr.
const RulesetDef *Core_Ruleset_Find(std::string_view name);

// The gametype a ruleset's text selects ("set g_gametype tdm"), or empty.
std::string Core_Ruleset_Gametype(const RulesetDef &def);

// The built-in marked standard for a gametype id, or nullptr.
const RulesetDef *Core_Ruleset_Standard(std::string_view gametype);

// Ruleset names are [a-z0-9_-], 1..32 characters, so a name can never reach
// outside rulesets/ when it becomes an exec path.
bool Core_Ruleset_ValidName(std::string_view name);

// The console text that applies `name`: a built-in's cfg, or
// "exec rulesets/<name>.cfg"; then "set g_ruleset <name>". Empty if the name
// is invalid.
std::string Core_Ruleset_Commands(std::string_view name);

// `ruleset` / `rulesets` from ServerCommand; true if handled.
bool Core_Rulesets_ServerCommand(const char *cmd);
