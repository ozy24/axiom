// Engine-facing part of the rulesets: the server commands.
#include "g_local.h"

#include "core/rulesets.h"

bool Core_Rulesets_ServerCommand(const char *cmd)
{
	if (!Q_strcasecmp(cmd, "rulesets"))
	{
		for (size_t i = 0; i < core_ruleset_count; ++i)
			gi.Com_PrintFmt("  {:<10} {}\n", core_rulesets[i].name, core_rulesets[i].description);
		gi.Com_Print("Others: rulesets/<name>.cfg in the game folder. Usage: sv ruleset <name>\n");
		return true;
	}
	if (Q_strcasecmp(cmd, "ruleset"))
		return false;

	if (gi.argc() < 3)
	{
		cvar_t *current = gi.cvar("g_ruleset", "stock", CVAR_SERVERINFO);
		gi.Com_PrintFmt("Ruleset: {}. Usage: sv ruleset <name> (sv rulesets lists them)\n", current->string);
		return true;
	}
	const char *name = gi.argv(2);
	const std::string commands = Core_Ruleset_Commands(name);
	if (commands.empty())
	{
		gi.Com_PrintFmt("Bad ruleset name \"{}\": use a-z, 0-9, _ and -\n", name);
		return true;
	}
	gi.cvar("g_ruleset", "stock", CVAR_SERVERINFO);
	gi.AddCommandString(commands.c_str());
	gi.Com_PrintFmt("Ruleset {} applied{}; latched settings take effect at the next map.\n", name,
	                Core_Ruleset_Find(name) ? "" : " (from rulesets/)");
	return true;
}
