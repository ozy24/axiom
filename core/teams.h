// Team locks and captains for the team gametypes (TDM, CTF; decision 0016,
// docs/design/teams.md), as pure logic. No engine access; the adapter is
// core/teams_register.cpp.
//
// Clients are identified by a key that outlives their connection (a human's
// social ID), so a player who drops and reconnects in another slot is still
// on a locked team's roster. Teams are 1 (red) and 2 (blue); 0 is no team.
//
// Locks and captains belong to the match, not to a round: only Clear (the
// match back to warmup), Reset (a new level) and the commands change them.
#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct TeamsPlayer
{
	std::string key;
	int team = 0; // 0 none, 1 red, 2 blue
	bool bot = false;
};

// Something to announce, from Sync and GoLive.
struct TeamsEvent
{
	enum class Kind
	{
		Captain,  // `key` is now the team's captain
		Locked,   // the match went live with match_lock: the team locked
		Unlocked, // the last human left a locked team: it unlocked
	};
	Kind kind;
	int team;
	std::string key;

	bool operator==(const TeamsEvent &o) const
	{
		return kind == o.kind && team == o.team && key == o.key;
	}
};

// Why a captain's command was refused, or Ok.
enum class TeamsResult
{
	Ok,
	NoTeam,      // the caller isn't on a team
	NotCaptain,  // ... or isn't its captain
	NotTeammate, // the target isn't on the caller's team
	Bot,         // the target is a bot (never captain, never kicked: the server's)
	Self,        // the target is the caller
	Already,     // locked already / unlocked already
	Empty,       // no players to lock a team for
};

class Teams
{
  public:
	// Bring the teams up to date with the clients now present (any order):
	// tenure for captains, a new captain when a team has none or its captain
	// left, the human on a locked team added to its roster (however they got
	// there), and a locked team with no humans left unlocked.
	std::vector<TeamsEvent> Sync(const std::vector<TeamsPlayer> &present);

	// Whether a client may join `team` now. Bots, no team (leaving) and an
	// unlocked team: yes; a locked team: only its roster.
	bool MayJoin(const std::string &key, int team, bool bot) const;

	// The match went live: with `match_lock`, lock every team with a human.
	std::vector<TeamsEvent> GoLive(bool match_lock);
	// The match is back in warmup: every lock off, rosters cleared. Captains stay.
	void Clear();
	// A new level: nothing kept (every human reconnects; the first back on a team is its captain).
	void Reset();

	// Lock: the humans on the team now become its roster. Unlock: the roster goes.
	TeamsResult Lock(int team);
	TeamsResult Unlock(int team);

	// A captain's commands, `by` the caller's key. Lock/unlock their own team;
	// hand the captaincy to a teammate; kick a teammate (the adapter moves them
	// to spectating; they're off the roster); the keys of their team to ready.
	TeamsResult CaptainLock(const std::string &by, bool lock);
	TeamsResult Transfer(const std::string &by, const std::string &target);
	TeamsResult Kick(const std::string &by, const std::string &target);
	TeamsResult ReadyTeam(const std::string &by, std::vector<std::string> &keys) const;

	bool Locked(int team) const;
	bool OnRoster(int team, const std::string &key) const;
	const std::string &Captain(int team) const; // empty: none
	int TeamOf(const std::string &key) const;   // 0 if not on a team (or not present)
	const std::vector<std::string> &Roster(int team) const;

  private:
	struct Member
	{
		std::string key;
		int team;
		bool bot;
		uint64_t since; // when they joined this team: lower has been on it longer
	};
	struct Side
	{
		bool locked = false;
		std::vector<std::string> roster;
		std::string captain;
	};

	const Member *Find(const std::string &key) const;
	TeamsResult CaptainOf(const std::string &by, int &team) const;
	bool HasHuman(int team) const;
	void AddToRoster(int team, const std::string &key);

	std::vector<Member> members_;
	std::array<Side, 3> sides_{}; // [1] red, [2] blue; [0] unused
	uint64_t clock_ = 0;
};

// `shuffle` (a warmup vote): a random split of the players on teams. `teams`
// holds each player's team now (1 or 2); the result is theirs after the
// shuffle, the sides' sizes differing by at most one. With three or more
// players it never gives the same two groups back, in either colour, so a
// passed shuffle always changes something.
std::vector<int> Core_Teams_Shuffle(const std::vector<int> &teams, std::mt19937 &rng);
