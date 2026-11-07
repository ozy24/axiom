// In-game admin (docs/design/admin.md, decision 0026), as pure logic.
//
// A player types `admin <password>` (admin_password; empty, the default,
// turns admin off) and may then run a fixed list of match commands with
// `admin <command>`: the console's `sv` match commands, plus kick and map.
// Never a cvar, `exec` or anything else the console can do, so a leaked
// password can spoil a match but not take the server.
//
// Guessing is slow: max_tries wrong passwords lock a player (by key, so a
// reconnect doesn't help) out for lockout_ms. An admin stays one until they
// leave the server: gone for longer than grace_ms (a map change under KEX
// reconnects everyone, which mustn't log them out). A key without a social ID
// ("slot:<n>") names a slot, not a person, so it goes the moment it's gone.
// A new password logs every admin out.
//
// Times are the adapter's real-time clock in ms, not level time: a timeout
// stops level time, and a `map` starts it again from zero. The state
// outlives the game module (`map` reloads it): Encode / Decode, in a cvar.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct AdminConfig
{
	int max_tries = 3;
	int64_t lockout_ms = 10 * 60 * 1000;
	int64_t grace_ms = 60 * 1000;
};

enum class AdminLogin
{
	Ok,
	Off,       // no admin_password
	Wrong,     // a wrong password: one try fewer
	LockedOut, // too many wrong ones; not checked
	Already,   // already an admin
};

// Equal, in time that doesn't depend on where they differ.
bool Core_Admin_SecretEquals(std::string_view a, std::string_view b);

class Admins
{
  public:
	explicit Admins(AdminConfig config = {}) : config_(config) {}

	// `admin <guess>` by the client with `key`.
	AdminLogin Login(const std::string &key, std::string_view guess, std::string_view password, int64_t now_ms);

	bool IsAdmin(const std::string &key) const
	{
		return admins_.count(key) != 0;
	}
	// Wrong guesses left before a lockout (after a Wrong), and how long a
	// lockout has to run (0 when none).
	int TriesLeft(const std::string &key, int64_t now_ms) const;
	int64_t LockedForMs(const std::string &key, int64_t now_ms) const;

	// Every frame, with the keys of every client here: an admin gone for
	// longer than the grace (a slot key: at all) stops being one, and spent
	// lockouts are forgotten. Returns the keys that stopped being admins.
	std::vector<std::string> Present(const std::vector<std::string> &here, int64_t now_ms);

	// A new password: nobody is an admin. Wrong guesses stay counted.
	void LogoutAll()
	{
		admins_.clear();
	}

	// "a:<gone ms>:<key> t:<wrong>:<last ms>:<key>", space-separated; a key
	// with a space in it isn't kept. Decode skips anything malformed.
	std::string Encode() const;
	static Admins Decode(std::string_view text, AdminConfig config = {});

	bool operator==(const Admins &o) const
	{
		return admins_ == o.admins_ && tries_ == o.tries_;
	}

  private:
	struct Tries
	{
		int wrong = 0;
		int64_t last_ms = 0;
		bool operator==(const Tries &o) const
		{
			return wrong == o.wrong && last_ms == o.last_ms;
		}
	};
	// Forgets a key's wrong guesses once lockout_ms has passed since the last.
	void Age(const std::string &key, int64_t now_ms);

	AdminConfig config_;
	std::map<std::string, int64_t> admins_; // key -> when it went (0: here)
	std::map<std::string, Tries> tries_;
};

// What `admin <command>` runs.
enum class AdminRoute
{
	Server, // the console's own `sv <command>`, as written
	Kick,   // the engine's kick, by client number
	Map,    // a map change, to a map on g_map_list
};

struct AdminCommand
{
	const char *name;
	const char *args; // usage, after the name ("" for none)
	const char *what; // one line for the list
	AdminRoute route;
	bool look; // only shows something: not announced to the players
};

// Every admin command, in the order `admin` lists them.
const std::vector<AdminCommand> &Core_Admin_Commands();
// The command `name` names (any case), or null: anything else is refused.
const AdminCommand *Core_Admin_FindCommand(std::string_view name);
// The list `admin` prints for an admin, one command a line.
std::string Core_Admin_Help();
