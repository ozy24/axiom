// Pure part of team locks and captains (core/teams.h). No engine access.
#include "core/teams.h"

#include <algorithm>
#include <numeric>

namespace
{
bool ValidTeam(int team)
{
	return team == 1 || team == 2;
}

// The same two groups, whichever colour each has.
bool SameGroups(const std::vector<int> &a, const std::vector<int> &b)
{
	bool same = true, swapped = true;
	for (size_t i = 0; i < a.size(); ++i)
	{
		same = same && a[i] == b[i];
		swapped = swapped && a[i] == 3 - b[i];
	}
	return same || swapped;
}
} // namespace

const Teams::Member *Teams::Find(const std::string &key) const
{
	for (const Member &m : members_)
	{
		if (m.key == key)
			return &m;
	}
	return nullptr;
}

bool Teams::HasHuman(int team) const
{
	return std::any_of(members_.begin(), members_.end(), [&](const Member &m) { return m.team == team && !m.bot; });
}

void Teams::AddToRoster(int team, const std::string &key)
{
	std::vector<std::string> &roster = sides_[team].roster;
	if (std::find(roster.begin(), roster.end(), key) == roster.end())
		roster.push_back(key);
}

std::vector<TeamsEvent> Teams::Sync(const std::vector<TeamsPlayer> &present)
{
	std::vector<Member> next;
	for (const TeamsPlayer &p : present)
	{
		if (!ValidTeam(p.team))
			continue;
		const Member *was = Find(p.key);
		const uint64_t since = was && was->team == p.team ? was->since : ++clock_;
		next.push_back({p.key, p.team, p.bot, since});
	}
	members_ = std::move(next);

	std::vector<TeamsEvent> events;
	for (int t = 1; t <= 2; ++t)
	{
		Side &side = sides_[t];
		if (side.locked)
		{
			if (!HasHuman(t))
			{
				// Nobody left to keep it for, and nobody to unlock it: a locked
				// empty team would keep the match from ever starting again.
				side.locked = false;
				side.roster.clear();
				events.push_back({TeamsEvent::Kind::Unlocked, t, {}});
			}
			else
			{
				// On the team however they got there (an unlock and relock, an
				// admin): they're one of its players now.
				for (const Member &m : members_)
				{
					if (m.team == t && !m.bot)
						AddToRoster(t, m.key);
				}
			}
		}

		const Member *captain = Find(side.captain);
		if (captain && captain->team == t && !captain->bot)
			continue;
		// The human who has been on the team longest.
		const Member *best = nullptr;
		for (const Member &m : members_)
		{
			if (m.team == t && !m.bot && (!best || m.since < best->since))
				best = &m;
		}
		side.captain = best ? best->key : std::string();
		if (best)
			events.push_back({TeamsEvent::Kind::Captain, t, best->key});
	}
	return events;
}

bool Teams::MayJoin(const std::string &key, int team, bool bot) const
{
	if (bot || !ValidTeam(team) || !sides_[team].locked)
		return true;
	return OnRoster(team, key);
}

std::vector<TeamsEvent> Teams::GoLive(bool match_lock)
{
	std::vector<TeamsEvent> events;
	if (!match_lock)
		return events;
	for (int t = 1; t <= 2; ++t)
	{
		if (Lock(t) == TeamsResult::Ok)
			events.push_back({TeamsEvent::Kind::Locked, t, {}});
	}
	return events;
}

void Teams::Clear()
{
	for (int t = 1; t <= 2; ++t)
	{
		sides_[t].locked = false;
		sides_[t].roster.clear();
	}
}

void Teams::Reset()
{
	*this = Teams();
}

TeamsResult Teams::Lock(int team)
{
	if (!ValidTeam(team))
		return TeamsResult::NoTeam;
	Side &side = sides_[team];
	if (side.locked)
		return TeamsResult::Already;
	if (!HasHuman(team))
		return TeamsResult::Empty;
	side.locked = true;
	side.roster.clear();
	for (const Member &m : members_)
	{
		if (m.team == team && !m.bot)
			side.roster.push_back(m.key);
	}
	return TeamsResult::Ok;
}

TeamsResult Teams::Unlock(int team)
{
	if (!ValidTeam(team))
		return TeamsResult::NoTeam;
	Side &side = sides_[team];
	if (!side.locked)
		return TeamsResult::Already;
	side.locked = false;
	side.roster.clear();
	return TeamsResult::Ok;
}

TeamsResult Teams::CaptainOf(const std::string &by, int &team) const
{
	const Member *m = Find(by);
	if (!m)
		return TeamsResult::NoTeam;
	team = m->team;
	return sides_[team].captain == by ? TeamsResult::Ok : TeamsResult::NotCaptain;
}

TeamsResult Teams::CaptainLock(const std::string &by, bool lock)
{
	int team = 0;
	const TeamsResult r = CaptainOf(by, team);
	if (r != TeamsResult::Ok)
		return r;
	return lock ? Lock(team) : Unlock(team);
}

TeamsResult Teams::Transfer(const std::string &by, const std::string &target)
{
	int team = 0;
	const TeamsResult r = CaptainOf(by, team);
	if (r != TeamsResult::Ok)
		return r;
	if (target == by)
		return TeamsResult::Self;
	const Member *m = Find(target);
	if (!m || m->team != team)
		return TeamsResult::NotTeammate;
	if (m->bot)
		return TeamsResult::Bot;
	sides_[team].captain = target;
	return TeamsResult::Ok;
}

TeamsResult Teams::Kick(const std::string &by, const std::string &target)
{
	int team = 0;
	const TeamsResult r = CaptainOf(by, team);
	if (r != TeamsResult::Ok)
		return r;
	if (target == by)
		return TeamsResult::Self;
	const Member *m = Find(target);
	if (!m || m->team != team)
		return TeamsResult::NotTeammate;
	if (m->bot)
		return TeamsResult::Bot;
	std::vector<std::string> &roster = sides_[team].roster;
	roster.erase(std::remove(roster.begin(), roster.end(), target), roster.end());
	return TeamsResult::Ok;
}

TeamsResult Teams::ReadyTeam(const std::string &by, std::vector<std::string> &keys) const
{
	int team = 0;
	const TeamsResult r = CaptainOf(by, team);
	if (r != TeamsResult::Ok)
		return r;
	keys.clear();
	for (const Member &m : members_)
	{
		if (m.team == team && !m.bot)
			keys.push_back(m.key);
	}
	return TeamsResult::Ok;
}

bool Teams::Locked(int team) const
{
	return ValidTeam(team) && sides_[team].locked;
}

bool Teams::OnRoster(int team, const std::string &key) const
{
	if (!ValidTeam(team))
		return false;
	const std::vector<std::string> &roster = sides_[team].roster;
	return std::find(roster.begin(), roster.end(), key) != roster.end();
}

const std::string &Teams::Captain(int team) const
{
	static const std::string none;
	return ValidTeam(team) ? sides_[team].captain : none;
}

int Teams::TeamOf(const std::string &key) const
{
	const Member *m = Find(key);
	return m ? m->team : 0;
}

const std::vector<std::string> &Teams::Roster(int team) const
{
	static const std::vector<std::string> none;
	return ValidTeam(team) ? sides_[team].roster : none;
}

std::vector<int> Core_Teams_Shuffle(const std::vector<int> &teams, std::mt19937 &rng)
{
	const size_t n = teams.size();
	std::vector<size_t> order(n);
	std::iota(order.begin(), order.end(), size_t{0});
	std::vector<int> out(n);
	// A repeat of the split now is redrawn; with three players a third of the
	// draws are one, so a few tries settle it.
	for (int tries = 0; tries < 64; ++tries)
	{
		std::shuffle(order.begin(), order.end(), rng);
		const size_t red = n / 2 + ((n % 2) ? (size_t)(rng() & 1) : 0); // an odd one out goes either way
		for (size_t i = 0; i < n; ++i)
			out[order[i]] = i < red ? 1 : 2;
		if (n < 3 || !SameGroups(out, teams))
			break;
	}
	return out;
}
