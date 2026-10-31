// Engine-facing part of the rulesets (docs/decisions/0013): the server
// commands, and applying server.cfg's starting mode.
#include "g_local.h"

#include "core/gametype.h"
#include "core/host_files.h"
#include "core/rulesets.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
cvar_t *g_ruleset;
cvar_t *g_ruleset_applied;
cvar_t *g_ruleset_variants;

void RegisterCvars()
{
	if (g_ruleset)
		return;
	g_ruleset = gi.cvar("g_ruleset", "", CVAR_SERVERINFO);
	// What this server last switched to: internal, so only the module sets it.
	g_ruleset_applied = gi.cvar("g_ruleset_applied", "", CVAR_NOSET);
	g_ruleset_variants = gi.cvar("g_ruleset_variants", "", CVAR_NOFLAGS);
}

std::vector<std::string> GametypeIds()
{
	std::vector<std::string> ids;
	for (size_t i = 0; i < core_gametype_count; ++i)
		ids.push_back(core_gametypes[i]->id);
	return ids;
}

std::string Title(const Ruleset &r)
{
	const GametypeDef *gt = Core_Gametype_Find(core_gametypes, core_gametype_count, r.gametype);
	const std::string title = gt ? gt->title : r.gametype;
	return r.variant.empty() ? title : title + ": " + r.variant;
}

void PrintUnknown(const char *name)
{
	gi.Com_PrintFmt("Unknown ruleset \"{}\". `sv rulesets` lists them; your own variants are "
	                "rulesets/<gametype>_<name>.cfg, listed in g_ruleset_variants.\n",
	                name);
}
// A cfg the engine would exec, read as it would: through Q2RED's file access
// when the engine offers it, else the plain path <basedir>/<game>/<file>.
bool ReadCfg(const std::string &file, std::string &text)
{
	const size_t slash = file.rfind('/');
	const std::string dir = slash == std::string::npos ? "" : file.substr(0, slash);
	const std::string name = slash == std::string::npos ? file : file.substr(slash + 1);
	if (Core_HostFiles_Available() && Core_HostFiles_Read(Core_GamePath(dir, name), text))
		return true;
	const char *basedir = gi.cvar("basedir", "", CVAR_NOFLAGS)->string;
	const char *game = gi.cvar("game", "", CVAR_NOFLAGS)->string;
	std::ifstream in(std::filesystem::path(basedir) / (*game ? game : "baseq2") / file, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream data;
	data << in.rdbuf();
	text = data.str();
	return true;
}

// Applies console text's `set` lines at once, following its `exec`s into
// their files (a few deep): what a switch's text would set, before the map
// spawns. The engine runs the text itself only once a player arrives.
void ApplySets(std::string_view text, int depth)
{
	for (const RulesetCfgStep &step : Core_Ruleset_ParseCfg(text))
	{
		if (!step.exec)
		{
			gi.cvar(step.name.c_str(), step.value.c_str(), CVAR_NOFLAGS);
			gi.cvar_forceset(step.name.c_str(), step.value.c_str());
			continue;
		}
		std::string file;
		if (depth < 4 && ReadCfg(step.name, file))
			ApplySets(file, depth + 1);
	}
}
} // namespace

std::optional<Ruleset> Core_Rulesets_Find(std::string_view name)
{
	RegisterCvars();
	return Core_Ruleset_Resolve(name, g_ruleset_variants->string, GametypeIds());
}

std::vector<Ruleset> Core_Rulesets_Available()
{
	RegisterCvars();
	return Core_Ruleset_All(g_ruleset_variants->string, GametypeIds());
}

std::string Core_Rulesets_Title(const Ruleset &r)
{
	return Title(r);
}

std::string Core_Rulesets_Switch(const Ruleset &r)
{
	RegisterCvars();
	gi.cvar_forceset("g_ruleset_applied", r.name.c_str());
	return Core_Ruleset_Commands(r);
}

bool Core_Rulesets_ServerCommand(const char *cmd)
{
	RegisterCvars();
	if (!Q_strcasecmp(cmd, "rulesets"))
	{
		for (const Ruleset &r : Core_Ruleset_All(g_ruleset_variants->string, GametypeIds()))
		{
			const char *what = r.builtin ? r.builtin->description : "your own: rulesets/";
			gi.Com_PrintFmt("  {:<14} {:<28} {}{}\n", r.name, Title(r), what, r.builtin ? "" : r.name + ".cfg");
		}
		gi.Com_Print("Change a gametype's settings in rulesets/<gametype>.cfg (only what differs).\n"
		             "Add a variant as rulesets/<gametype>_<name>.cfg and list it in g_ruleset_variants.\n"
		             "Usage: sv ruleset <name>\n");
		return true;
	}
	if (Q_strcasecmp(cmd, "ruleset"))
		return false;

	if (gi.argc() < 3)
	{
		gi.Com_PrintFmt("Ruleset: {}. Usage: sv ruleset <name> (sv rulesets lists them)\n",
		                *g_ruleset->string ? g_ruleset->string : "none (server.cfg's settings)");
		return true;
	}
	const char *name = gi.argv(2);
	const std::optional<Ruleset> r = Core_Ruleset_Resolve(name, g_ruleset_variants->string, GametypeIds());
	if (!r)
	{
		PrintUnknown(name);
		return true;
	}
	gi.AddCommandString(Core_Rulesets_Switch(*r).c_str());
	gi.Com_PrintFmt("Ruleset {} ({}) applied: its settings now, its gametype at the next map (`map <name>`).\n",
	                r->name, Title(*r));
	return true;
}

void Core_Rulesets_PreInit()
{
	RegisterCvars();
	if (!*g_ruleset->string || !strcmp(g_ruleset->string, g_ruleset_applied->string))
		return;
	const std::string name = g_ruleset->string;
	const std::optional<Ruleset> r = Core_Ruleset_Resolve(name, g_ruleset_variants->string, GametypeIds());
	gi.cvar_forceset("g_ruleset_applied", name.c_str()); // once, even if it's unknown
	if (!r)
	{
		PrintUnknown(name.c_str());
		return;
	}
	// sv commands don't exist before the first map, so server.cfg names its
	// starting mode with `set g_ruleset`. Its settings go in now, the admin's
	// files' `set` lines too (the gametype is latched and read right after
	// this; items and the like as the map spawns). The engine holds the
	// queued switch until a player arrives (it defers what's queued while a
	// map loads; BB, 2026-10-30), then runs it whole.
	const std::string text = Core_Rulesets_Switch(*r);
	ApplySets(text, 0);
	gi.AddCommandString(text.c_str());
	gi.Com_PrintFmt("Ruleset: starting {} ({})\n", r->name, Title(*r));
}
