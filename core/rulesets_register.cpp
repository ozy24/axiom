// Engine-facing part of the rulesets (docs/decisions/0013): the server
// commands, and applying server.cfg's starting mode.
#include "g_local.h"

#include "core/gametype.h"
#include "core/rulesets.h"

namespace
{
cvar_t *g_ruleset;
cvar_t *g_ruleset_applied;
cvar_t *g_rulesets;

void RegisterCvars()
{
	if (g_ruleset)
		return;
	g_ruleset = gi.cvar("g_ruleset", "", CVAR_SERVERINFO);
	g_ruleset_applied = gi.cvar("g_ruleset_applied", "", CVAR_NOFLAGS); // what this server last switched to
	g_rulesets = gi.cvar("g_rulesets", "", CVAR_NOFLAGS);
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
	                "rulesets/<gametype>_<name>.cfg, listed in g_rulesets.\n",
	                name);
}
} // namespace

std::optional<Ruleset> Core_Rulesets_Find(std::string_view name)
{
	RegisterCvars();
	return Core_Ruleset_Resolve(name, g_rulesets->string, GametypeIds());
}

std::vector<Ruleset> Core_Rulesets_Available()
{
	RegisterCvars();
	return Core_Ruleset_All(g_rulesets->string, GametypeIds());
}

std::string Core_Rulesets_Title(const Ruleset &r)
{
	return Title(r);
}

bool Core_Rulesets_ServerCommand(const char *cmd)
{
	RegisterCvars();
	if (!Q_strcasecmp(cmd, "rulesets"))
	{
		for (const Ruleset &r : Core_Ruleset_All(g_rulesets->string, GametypeIds()))
		{
			const char *what = r.builtin ? r.builtin->description : "your own: rulesets/";
			gi.Com_PrintFmt("  {:<14} {:<28} {}{}\n", r.name, Title(r), what, r.builtin ? "" : r.name + ".cfg");
		}
		gi.Com_Print("Change a gametype's settings in rulesets/<gametype>.cfg (only what differs).\n"
		             "Add a variant as rulesets/<gametype>_<name>.cfg and list it in g_rulesets.\n"
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
	const std::optional<Ruleset> r = Core_Ruleset_Resolve(name, g_rulesets->string, GametypeIds());
	if (!r)
	{
		PrintUnknown(name);
		return true;
	}
	gi.AddCommandString(Core_Ruleset_Commands(*r).c_str());
	gi.Com_PrintFmt("Ruleset {} ({}) applied; it takes effect at the next map.\n", r->name, Title(*r));
	return true;
}

void Core_Rulesets_PreInit()
{
	RegisterCvars();
	if (!*g_ruleset->string || !strcmp(g_ruleset->string, g_ruleset_applied->string))
		return;
	const std::string name = g_ruleset->string;
	const std::optional<Ruleset> r = Core_Ruleset_Resolve(name, g_rulesets->string, GametypeIds());
	gi.cvar_forceset("g_ruleset_applied", name.c_str()); // once, even if it's unknown
	if (!r)
	{
		PrintUnknown(name.c_str());
		return;
	}
	// sv commands don't exist before the first map, so server.cfg names its
	// starting mode with `set g_ruleset`. Its built-in settings go in now (the
	// gametype is latched and read right after this); the queued switch
	// repeats them in order around the admin's execs.
	for (const auto &[cvar, value] : Core_Ruleset_Settings(*r))
	{
		gi.cvar(cvar.c_str(), value.c_str(), CVAR_NOFLAGS);
		gi.cvar_forceset(cvar.c_str(), value.c_str());
	}
	gi.AddCommandString(Core_Ruleset_Commands(*r).c_str());
	gi.Com_PrintFmt("Ruleset: starting {} ({})\n", r->name, Title(*r));
}
