// Rulesets: a gametype's gameplay settings, and its variants
// (docs/decisions/0013; .cfg text through the engine, decision 0003).
//
//   server.cfg                  the server: hostname, voting...; `set g_ruleset duel`
//                               picks the mode it starts in
//   rulesets/<gametype>.cfg     the admin's changes to a gametype's built-in settings
//   rulesets/<gametype>_<x>.cfg an admin's own variant, offered for votes by
//                               listing it in g_ruleset_variants
//
//   sv ruleset <name>   switch: the settings now, the gametype (latched) at the next map
//   sv rulesets         list them
//
// Every switch starts from the same place: every gameplay cvar back to its
// compiled default, then the gametype's built-in settings and its cfg, then
// the variant's. Nothing one mode sets leaks into the next.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct RulesetDef
{
	const char *name;
	const char *gametype; // the g_gametype it plays
	const char *description;
	const char *cfg;               // console commands, one per line
	const char *variant = nullptr; // null: the gametype's own settings; else the variant's name,
	                               // voted as "<gametype>: <variant>" ("Duel: HoonyMode")
};

extern const RulesetDef core_rulesets[];
extern const size_t core_ruleset_count;

// The gameplay cvars a switch resets, with the defaults their modules
// register (keep in step). A cvar an admin's cfg sets that isn't here is
// not reset, so add every new gameplay cvar.
struct GameplayCvar
{
	const char *name;
	const char *value;
};
extern const GameplayCvar core_gameplay_cvars[];
extern const size_t core_gameplay_cvar_count;

// Built-in by exact name, or nullptr.
const RulesetDef *Core_Ruleset_Find(std::string_view name);

// Ruleset names are [a-z0-9_-], 1..32 characters, so a name can never reach
// outside rulesets/ when it becomes an exec path.
bool Core_Ruleset_ValidName(std::string_view name);

// A ruleset, built-in or the admin's.
struct Ruleset
{
	std::string name;     // "hoony", "ctf_instagib"
	std::string gametype; // "duel", "ctf"
	std::string variant;  // empty for a gametype's own settings; "HoonyMode", "Instagib"
	const RulesetDef *builtin = nullptr;
};

// `name` as a ruleset: a built-in, or an admin's variant listed in
// g_ruleset_variants and named <gametype>_<variant> after a known gametype (its
// label: "ctf_instagib" is "Instagib", "ffa_rocket_arena" "Rocket Arena").
std::optional<Ruleset> Core_Ruleset_Resolve(std::string_view name, std::string_view g_ruleset_variants,
                                            const std::vector<std::string> &gametypes);

// Every ruleset, by gametype in `gametypes` order: each gametype's own
// first, then its built-in variants, then the admin's.
std::vector<Ruleset> Core_Ruleset_All(std::string_view g_ruleset_variants, const std::vector<std::string> &gametypes);

// The console text for a switch: the reset, the gametype's built-in settings
// and `exec rulesets/<gametype>.cfg`, then for a variant its built-in
// settings and `exec rulesets/<name>.cfg`; then g_ruleset. (g_ruleset_applied
// is write-protected: Core_Rulesets_Switch sets it.)
std::string Core_Ruleset_Commands(const Ruleset &ruleset);

// One step of console text, as Core_Ruleset_ParseCfg reads it: a cvar it
// sets, or a file it execs.
struct RulesetCfgStep
{
	bool exec = false;
	std::string name;  // the cvar, or the file for an exec
	std::string value; // a set's value
};

// The `set` (`seta`, `sets`, `setu`) and `exec` commands in console text,
// in order, split as the console splits it: on newlines and on semicolons
// outside quotes (even inside a // comment), then a // comment dropped.
// Anything else (a bare `fraglimit 30`, other commands) is left out: it runs
// when the engine runs the text.
std::vector<RulesetCfgStep> Core_Ruleset_ParseCfg(std::string_view text);

// From the engine (core/rulesets_register.cpp): with this server's
// g_ruleset_variants and gametypes. The title is what players see: "Duel",
// "Duel: HoonyMode".
std::optional<Ruleset> Core_Rulesets_Find(std::string_view name);
std::vector<Ruleset> Core_Rulesets_Available();
std::string Core_Rulesets_Title(const Ruleset &ruleset);
// A switch to `ruleset`: records it as applied (g_ruleset_applied, which the
// console can't set) and returns Core_Ruleset_Commands' text to queue.
std::string Core_Rulesets_Switch(const Ruleset &ruleset);

// `ruleset` / `rulesets` from ServerCommand; true if handled.
bool Core_Rulesets_ServerCommand(const char *cmd);

// From Core_Gametype_PreInit, as the game loads and before the gametype is
// chosen: if g_ruleset names a mode not yet applied (server.cfg's starting
// mode, or an rcon `set g_ruleset` before a `map`), sets its built-in
// settings at once, so this level starts in it, and queues the full switch
// for the admin's cfgs (they land during warmup; the match flow takes
// settings that change before the match starts).
void Core_Rulesets_PreInit();
