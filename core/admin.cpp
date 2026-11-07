// Pure part of in-game admin (core/admin.h). No engine access.
#include "core/admin.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

bool Core_Admin_SecretEquals(std::string_view a, std::string_view b)
{
	size_t diff = a.size() ^ b.size();
	const size_t n = std::max(a.size(), b.size());
	for (size_t i = 0; i < n; ++i)
	{
		const unsigned char x = i < a.size() ? (unsigned char)a[i] : 0;
		const unsigned char y = i < b.size() ? (unsigned char)b[i] : 0;
		diff |= (size_t)(x ^ y);
	}
	return diff == 0;
}

void Admins::Age(const std::string &key, int64_t now_ms)
{
	const auto it = tries_.find(key);
	if (it != tries_.end() && now_ms - it->second.last_ms >= config_.lockout_ms)
		tries_.erase(it);
}

AdminLogin Admins::Login(const std::string &key, std::string_view guess, std::string_view password, int64_t now_ms)
{
	if (password.empty())
		return AdminLogin::Off;
	if (IsAdmin(key))
		return AdminLogin::Already;
	Age(key, now_ms);
	if (LockedForMs(key, now_ms) > 0)
		return AdminLogin::LockedOut;
	if (!Core_Admin_SecretEquals(guess, password))
	{
		Tries &t = tries_[key];
		++t.wrong;
		t.last_ms = now_ms;
		return AdminLogin::Wrong;
	}
	tries_.erase(key);
	admins_[key] = 0;
	return AdminLogin::Ok;
}

int Admins::TriesLeft(const std::string &key, int64_t now_ms) const
{
	const auto it = tries_.find(key);
	if (it == tries_.end() || now_ms - it->second.last_ms >= config_.lockout_ms)
		return config_.max_tries;
	return std::max(0, config_.max_tries - it->second.wrong);
}

int64_t Admins::LockedForMs(const std::string &key, int64_t now_ms) const
{
	const auto it = tries_.find(key);
	if (it == tries_.end() || it->second.wrong < config_.max_tries)
		return 0;
	return std::max<int64_t>(0, it->second.last_ms + config_.lockout_ms - now_ms);
}

std::vector<std::string> Admins::Present(const std::vector<std::string> &here, int64_t now_ms)
{
	std::vector<std::string> gone;
	for (auto it = admins_.begin(); it != admins_.end();)
	{
		const bool present = std::find(here.begin(), here.end(), it->first) != here.end();
		if (present)
			it->second = 0;
		else if (it->second == 0)
			it->second = now_ms;
		const bool slot = it->first.rfind("slot:", 0) == 0;
		if (!present && (slot || now_ms - it->second > config_.grace_ms))
		{
			gone.push_back(it->first);
			it = admins_.erase(it);
		}
		else
			++it;
	}
	for (auto it = tries_.begin(); it != tries_.end();)
	{
		if (now_ms - it->second.last_ms >= config_.lockout_ms)
			it = tries_.erase(it);
		else
			++it;
	}
	return gone;
}

namespace
{
bool Keepable(const std::string &key)
{
	return !key.empty() && std::none_of(key.begin(), key.end(), [](char c) { return isspace((unsigned char)c); });
}

// Splits "<n>:<rest>" into the number and the rest; false if malformed.
bool Number(std::string_view &text, int64_t &out)
{
	const size_t colon = text.find(':');
	if (colon == std::string_view::npos || colon == 0)
		return false;
	const std::string digits(text.substr(0, colon));
	const size_t first = digits[0] == '-' ? 1 : 0;
	if (first >= digits.size() || digits.find_first_not_of("0123456789", first) != std::string::npos)
		return false;
	out = std::strtoll(digits.c_str(), nullptr, 10);
	text.remove_prefix(colon + 1);
	return true;
}
} // namespace

std::string Admins::Encode() const
{
	std::string out;
	const auto add = [&out](const std::string &word)
	{
		if (!out.empty())
			out += ' ';
		out += word;
	};
	for (const auto &[key, gone] : admins_)
		if (Keepable(key))
			add("a:" + std::to_string(gone) + ':' + key);
	for (const auto &[key, t] : tries_)
		if (Keepable(key))
			add("t:" + std::to_string(t.wrong) + ':' + std::to_string(t.last_ms) + ':' + key);
	return out;
}

Admins Admins::Decode(std::string_view text, AdminConfig config)
{
	Admins out(config);
	size_t pos = 0;
	while (pos < text.size())
	{
		const size_t end = std::min(text.find(' ', pos), text.size());
		std::string_view word = text.substr(pos, end - pos);
		pos = end + 1;
		if (word.size() < 3 || word[1] != ':')
			continue;
		const char kind = word[0];
		word.remove_prefix(2);
		int64_t a = 0, b = 0;
		if (kind == 'a' && Number(word, a) && !word.empty())
			out.admins_[std::string(word)] = a;
		else if (kind == 't' && Number(word, a) && Number(word, b) && !word.empty() && a > 0)
			out.tries_[std::string(word)] = {(int)a, b};
	}
	return out;
}

const std::vector<AdminCommand> &Core_Admin_Commands()
{
	static const std::vector<AdminCommand> commands = {
	    {"match", "", "the match: its state, scores and players", AdminRoute::Server, true},
	    {"teams", "", "the teams, their captains and locks", AdminRoute::Server, true},
	    {"maplist", "", "the maps of g_map_list", AdminRoute::Server, true},
	    {"rulesets", "", "the modes this server offers", AdminRoute::Server, true},
	    {"readyall", "", "in a ready-up warmup: everyone playing is ready", AdminRoute::Server, false},
	    {"readyteam", "red|blue", "in a ready-up warmup: that team is ready", AdminRoute::Server, false},
	    {"lockteam", "[red|blue]", "lock a team (no team: both)", AdminRoute::Server, false},
	    {"unlockteam", "[red|blue]", "unlock a team (no team: both)", AdminRoute::Server, false},
	    {"shuffle", "", "in a team game's warmup: new, even teams", AdminRoute::Server, false},
	    {"setteam", "<player> red|blue", "put a player on a team (not past a lock)", AdminRoute::Server, false},
	    {"spectate", "<player>", "take a player out of play", AdminRoute::Server, false},
	    {"timeout", "", "stop a match under way (no allowance used)", AdminRoute::Server, false},
	    {"timein", "", "end the timeout under way", AdminRoute::Server, false},
	    {"endmatch", "", "end the match under way now, as it stands", AdminRoute::Server, false},
	    {"resetmatch", "", "back to warmup, every score zeroed", AdminRoute::Server, false},
	    {"callvote", "<kind> [choice]", "put a vote to the players", AdminRoute::Server, false},
	    {"passvote", "", "pass the vote under way", AdminRoute::Server, false},
	    {"cancelvote", "", "cancel the vote under way", AdminRoute::Server, false},
	    {"ruleset", "<name>", "switch mode: its settings now, its gametype at the next map", AdminRoute::Server, false},
	    {"map", "<name>", "change to a map on g_map_list", AdminRoute::Map, false},
	    {"kick", "<player>", "remove a player from the server", AdminRoute::Kick, false},
	};
	return commands;
}

const AdminCommand *Core_Admin_FindCommand(std::string_view name)
{
	for (const AdminCommand &c : Core_Admin_Commands())
	{
		const std::string_view n = c.name;
		if (n.size() == name.size() && std::equal(n.begin(), n.end(), name.begin(), [](char a, char b)
		                                          { return tolower((unsigned char)a) == tolower((unsigned char)b); }))
			return &c;
	}
	return nullptr;
}

std::string Core_Admin_Help()
{
	std::string out = "Admin commands (admin <command>):\n";
	for (const AdminCommand &c : Core_Admin_Commands())
	{
		std::string usage = c.name;
		if (*c.args)
			usage += std::string(" ") + c.args;
		usage.resize(std::max<size_t>(usage.size(), 26), ' ');
		out += "  " + usage + " " + c.what + "\n";
	}
	return out;
}
