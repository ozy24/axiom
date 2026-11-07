// Engine-facing part of in-game admin (core/admin.h): the cvars, `admin`, and
// running an admin's command as the console's own, with its replies sent to
// the admin as well.
#include "g_local.h"

#include "core/admin.h"
#include "core/admin_hooks.h"
#include "core/player_name.h"
#include "core/vote.h"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace
{
cvar_t *admin_password, *g_admin_memory, *g_map_list;
Admins admins;
std::string saved;         // g_admin_memory as last written
std::string password_seen; // admin_password as last seen: a change logs every admin out
bool loaded = false;
// A kick or map change waits for the next game frame, as a passed vote's
// does: run from inside the client's own command, a `map` strands the
// players reconnecting (seen with simulated clients, 2026-11-02).
std::string pending;
std::vector<std::pair<int, std::string>> kicks; // slot, key: still the same player when it runs

int64_t NowMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

void Save()
{
	const std::string text = admins.Encode();
	if (text == saved)
		return;
	saved = text;
	gi.cvar_forceset("g_admin_memory", text.c_str());
}

void WarnIfRcon()
{
	const cvar_t *rcon = gi.cvar("rcon_password", "", CVAR_NOFLAGS);
	if (*admin_password->string && !strcmp(admin_password->string, rcon->string))
		gi.Com_Print("Admin: WARNING: admin_password is the same as rcon_password. An admin's password "
		             "should never also open the console.\n");
}

// Loads the state once (it outlives the module in g_admin_memory), notices a
// new password, and drops admins who have left.
void Sync()
{
	if (!loaded)
	{
		admin_password = gi.cvar("admin_password", "", CVAR_NOFLAGS);
		g_admin_memory = gi.cvar("g_admin_memory", "", CVAR_NOSET); // internal: only the module sets it
		g_map_list = gi.cvar("g_map_list", "", CVAR_NOFLAGS);
		admins = Admins::Decode(g_admin_memory->string);
		saved = g_admin_memory->string;
		password_seen = admin_password->string;
		loaded = true;
		WarnIfRcon();
	}
	if (password_seen != admin_password->string)
	{
		password_seen = admin_password->string;
		admins.LogoutAll();
		gi.Com_PrintFmt("Admin: {}; every admin logged out\n",
		                *admin_password->string ? "the password changed" : "admin is off (no admin_password)");
		WarnIfRcon();
	}
	std::vector<std::string> here;
	for (uint32_t i = 1; i <= game.maxclients; ++i)
	{
		const edict_t *ent = g_edicts + i;
		if (ent->inuse && ent->client && ent->client->pers.connected && !(ent->svflags & SVF_BOT))
			here.push_back(Core_PlayerKey(ent));
	}
	for (const std::string &key : admins.Present(here, NowMs()))
		gi.Com_PrintFmt("Admin: {} left; no longer an admin\n", key);
	Save();
}

void Tell(edict_t *ent, const std::string &text)
{
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", text.c_str());
}

// While an admin's command runs, the console's replies go to them too.
edict_t *reply_to;
void (*console_print)(const char *msg);

void ReplyPrint(const char *msg)
{
	console_print(msg);
	if (!reply_to)
		return;
	std::string text = msg;
	// The console's usage lines name `sv`; the admin types `admin`.
	for (size_t at = text.find("sv "); at != std::string::npos; at = text.find("sv ", at + 6))
		if (at == 0 || text[at - 1] == ' ' || text[at - 1] == '(')
			text.replace(at, 3, "admin ");
	gi.LocClient_Print(reply_to, PRINT_HIGH, "{}", text.c_str());
}

void RunAsServer(edict_t *ent)
{
	reply_to = ent;
	console_print = gi.Com_Print;
	gi.Com_Print = ReplyPrint;
	ServerCommand(); // reads gi.argv(1) on: `admin <cmd> ...` lines up with `sv <cmd> ...`
	gi.Com_Print = console_print;
	reply_to = nullptr;
}

// Everyone sees who did what: the admin's whole command.
void Announce(edict_t *ent)
{
	std::string line = gi.args();
	line.erase(0, line.find_first_not_of(' '));
	gi.LocBroadcast_Print(PRINT_HIGH, "{} (admin): {}\n", ent->client->pers.netname, line.c_str());
}

// The words after the command: a player's name may have spaces.
std::string Rest()
{
	if (gi.argc() == 3)
		return gi.argv(2);
	std::string rest = gi.args() + strlen(gi.argv(1));
	rest.erase(0, rest.find_first_not_of(' '));
	return rest;
}

void Kick(edict_t *ent)
{
	const std::string arg = Rest();
	edict_t *target = arg.empty() ? nullptr : Core_PlayerNamed(arg.c_str());
	if (arg.empty())
		Tell(ent, "Usage: admin kick <player> (name or client number)");
	else if (!target)
		Tell(ent, "No one player called '" + arg + "'.");
	else if (target->svflags & SVF_BOT)
		Tell(ent, "Bots are the server's to place: use bot_count.");
	else if (target == ent)
		Tell(ent, "Not yourself.");
	else
	{
		Announce(ent);
		gi.Com_PrintFmt("Admin: {} kicked {}\n", Core_PlayerName(ent), Core_PlayerName(target));
		kicks.emplace_back((int)(target - g_edicts) - 1, Core_PlayerKey(target));
	}
}

void Map(edict_t *ent)
{
	const std::vector<std::string> maps = Core_Vote_MapList(g_map_list->string, ctf->integer != 0);
	std::string list;
	for (const std::string &m : maps)
		list += (list.empty() ? "" : " ") + m;
	const char *want = gi.argc() > 2 ? gi.argv(2) : "";
	for (const std::string &m : maps)
	{
		if (Q_strcasecmp(m.c_str(), want))
			continue;
		Announce(ent);
		gi.Com_PrintFmt("Admin: {} changed the map to {}\n", Core_PlayerName(ent), m);
		pending += "map " + m + "\n"; // a name off the list: never anything else
		return;
	}
	Tell(ent, std::string(*want ? "Not on this server's map list. " : "Usage: admin map <name>. ") + "Maps: " + list);
}

void Login(edict_t *ent, const std::string &key, const char *word)
{
	if (!*admin_password->string)
	{
		Tell(ent, "Admin is off on this server.");
		return;
	}
	if (!*word)
	{
		Tell(ent, "Usage: admin <password>");
		return;
	}
	if (Core_Admin_FindCommand(word))
	{
		// Not a guess: no try spent.
		Tell(ent, "You aren't an admin: log in first with `admin <password>`.");
		return;
	}
	char ip[MAX_INFO_VALUE] = {0};
	gi.Info_ValueForKey(ent->client->pers.userinfo, "ip", ip, sizeof(ip));
	const int64_t now = NowMs();
	switch (admins.Login(key, word, admin_password->string, now))
	{
	case AdminLogin::Ok:
		Tell(ent, "You are an admin until you leave. `admin` lists the commands.");
		gi.LocBroadcast_Print(PRINT_HIGH, "{} is now an admin.\n", ent->client->pers.netname);
		gi.Com_PrintFmt("Admin: {} ({}, {}) logged in\n", Core_PlayerName(ent), key, ip);
		break;
	case AdminLogin::Wrong:
	{
		const int left = admins.TriesLeft(key, now);
		Tell(ent, left ? fmt::format("Wrong password ({} {} left).", left, left == 1 ? "try" : "tries")
		               : std::string("Wrong password. Locked out for 10 minutes."));
		gi.Com_PrintFmt("Admin: wrong password from {} ({}, {}){}\n", Core_PlayerName(ent), key, ip,
		                left ? "" : "; locked out");
		break;
	}
	case AdminLogin::LockedOut:
	{
		const int64_t min = (admins.LockedForMs(key, now) + 59999) / 60000;
		Tell(ent, fmt::format("Too many wrong passwords: try again in {} minute{}.", min, min == 1 ? "" : "s"));
		gi.Com_PrintFmt("Admin: {} ({}, {}) tried while locked out\n", Core_PlayerName(ent), key, ip);
		break;
	}
	case AdminLogin::Off:
	case AdminLogin::Already:
		break;
	}
	Save();
}

void Run(edict_t *ent, const char *word)
{
	if (!*word)
	{
		std::string help = Core_Admin_Help();
		for (size_t at = 0; at < help.size();)
		{
			const size_t end = help.find('\n', at);
			Tell(ent, help.substr(at, end - at));
			at = end == std::string::npos ? help.size() : end + 1;
		}
		return;
	}
	const AdminCommand *c = Core_Admin_FindCommand(word);
	if (!c)
	{
		Tell(ent, "Not an admin command. `admin` lists them.");
		return;
	}
	std::string line = gi.args();
	line.erase(0, line.find_first_not_of(' '));
	gi.Com_PrintFmt("Admin: {}: {}\n", Core_PlayerName(ent), line);
	switch (c->route)
	{
	case AdminRoute::Server:
		if (!c->look) // just looking isn't news
			Announce(ent);
		RunAsServer(ent);
		break;
	case AdminRoute::Kick:
		Kick(ent);
		break;
	case AdminRoute::Map:
		Map(ent);
		break;
	}
}
} // namespace

void Core_Admin_Frame()
{
	for (const auto &[slot, key] : kicks)
	{
		const edict_t *target = g_edicts + slot + 1;
		if (target->inuse && target->client && Core_PlayerKey(target) == key)
			pending += fmt::format("kick {}\n", slot);
	}
	kicks.clear();
	if (!pending.empty())
	{
		gi.AddCommandString(pending.c_str());
		pending.clear();
	}
	if (deathmatch->integer)
		Sync();
}

bool Core_Admin_ClientCommand(edict_t *ent, const char *cmd)
{
	if (!deathmatch->integer || Q_strcasecmp(cmd, "admin"))
		return false;
	if (ent->svflags & SVF_BOT)
		return true;
	Sync();
	const std::string key = Core_PlayerKey(ent);
	const char *word = gi.argc() > 1 ? gi.argv(1) : "";
	if (admins.IsAdmin(key))
		Run(ent, word);
	else
		Login(ent, key, word);
	return true;
}
