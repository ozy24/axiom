// Voting (docs/design/voting.md), as pure logic: what can be voted on, who
// counts, and when a vote passes or fails. No engine access.
//
// Three kinds: map, gametype and restart (the current map, reloaded). A
// gametype vote names a ruleset: the gametype's standard one or a variant
// (HoonyMode is Duel's); "ruleset" is accepted as another word for it.
// Only voters count: humans in the match (in Duel, everyone in the queue).
// The caller's vote is a yes. A vote passes on a majority of the voters there
// now, fails as soon as it can't pass or after the timeout, and runs a few
// seconds after it passes. Each player calls at most a few votes per map and
// waits after one of theirs fails. Votes can be called mid-match (a match
// started with the wrong settings is voted away); every kind reloads a map,
// so the match starts again from warmup.
//
// The adapter (core/vote_register.cpp) carries out the passed proposal and
// shows the vote on the HUD's status line; the Tab menu issues the same
// commands as the console.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

enum class VoteKind
{
	Map,
	Gametype,
	Restart,
};

// "map", "gametype", "restart"; and back, case-insensitively ("ruleset" too,
// for gametype).
const char *VoteKindName(VoteKind kind);
std::optional<VoteKind> Core_Vote_ParseKind(std::string_view name);

struct VoteProposal
{
	VoteKind kind = VoteKind::Restart;
	std::string arg;   // the map or ruleset name; empty for restart
	std::string label; // shown instead of arg if set: "Duel: HoonyMode"

	bool operator==(const VoteProposal &o) const
	{
		return kind == o.kind && arg == o.arg;
	}
};

// "map q2dm3", "gametype Duel: HoonyMode", "restart".
std::string Core_Vote_Describe(const VoteProposal &p);

// The words of a list cvar (g_map_list, g_vote_allow), lowercased; anything
// that isn't [a-z0-9_-] (up to 32 characters) is dropped, so a word can never
// break a console command or a path.
std::vector<std::string> Core_Vote_Words(std::string_view text);

// The maps a vote may pick: g_map_list if it names any (Core_Maps_Words:
// "q64/dm1" too), else the stock maps for the family (q2ctf1-5 for CTF, q2dm1-8 otherwise).
std::vector<std::string> Core_Vote_MapList(std::string_view g_map_list, bool ctf);

// Where a gametype or ruleset change reloads: the current map if the new
// list has it, else the list's first.
std::string Core_Vote_MapFor(const std::vector<std::string> &list, std::string_view current);

struct VoteConfig
{
	bool enabled = true;              // g_vote
	std::set<VoteKind> allowed;       // g_vote_allow
	bool midgame = true;              // g_vote_midgame: also during a live match
	int64_t timeout_ms = 30000;       // g_vote_time
	int64_t pass_delay_ms = 3000;     // between "passed" and running it
	int64_t fail_cooldown_ms = 30000; // g_vote_cooldown: after a caller's vote fails
	int call_limit = 3;               // g_vote_limit: calls per player per map; 0 = no limit
};

// What a vote can choose from, from the server (all lowercase).
struct VoteChoices
{
	std::vector<std::string> maps;
	std::vector<std::string> gametypes; // ruleset names
};

enum class VoteError
{
	None,
	Disabled,     // voting is off
	NotAllowed,   // this kind isn't in g_vote_allow
	MatchLive,    // only outside a live match
	InProgress,   // another vote is open
	NotVoter,     // spectators and bots don't vote
	LimitReached, // this player's calls for the map are used up
	Cooldown,     // this player's last vote failed too recently
	BadArg,       // not on the list (map, gametype, ruleset) or an argument to restart
	NoVote,       // `vote` with nothing open
	AlreadyVoted, // the same answer again
};

// The player's message for an error (empty for None).
std::string Core_Vote_ErrorText(VoteError e, const VoteConfig &config, const VoteProposal &p,
                                const VoteChoices &choices);

enum class VoteEvent
{
	None,
	Passed,   // announce; it runs after the pass delay
	Failed,   // a majority against it
	TimedOut, // no majority either way in time
	Execute,  // run the proposal now; the vote is over
};

struct VoteTally
{
	int yes = 0;
	int no = 0;
	int voters = 0;
};

class Vote
{
  public:
	explicit Vote(VoteConfig config = {}) : config_(std::move(config)) {}

	const VoteConfig &Config() const
	{
		return config_;
	}
	void SetConfig(VoteConfig config)
	{
		config_ = std::move(config);
	}

	// A new level: any open vote is gone and the call limits start again.
	void Reset();

	// The caller of an admin's `sv callvote`: not a voter, so it has no answer,
	// and g_vote, g_vote_allow, midgame, the call limit and the cooldown don't
	// apply; it still needs no vote open and a choice on the lists.
	static constexpr int kServer = -1;

	// `callvote`: checks it and opens it with the caller's yes. `live`: a match
	// is being played (votes then need midgame); `voters`: who counts now.
	VoteError Call(int caller, VoteProposal proposal, const VoteChoices &choices, bool live,
	               const std::vector<int> &voters, int64_t now_ms);

	// `vote yes|no`: a voter's answer; they may change it.
	VoteError Cast(int id, bool yes, const std::vector<int> &voters);

	// A client left: their answer, calls and cooldown go, so the slot's next
	// client starts fresh.
	void Forget(int id)
	{
		answers_.erase(id);
		calls_.erase(id);
		failed_at_ms_.erase(id);
	}

	// Every frame: recounts against who counts now (leavers drop out), then
	// passes, fails, times out, or runs.
	VoteEvent Update(const std::vector<int> &voters, int64_t now_ms);

	// Open (being voted on) or passed (waiting to run).
	bool Open() const
	{
		return state_ == State::Open;
	}
	bool Passed() const
	{
		return state_ == State::Passed;
	}
	const VoteProposal &Proposal() const
	{
		return proposal_;
	}
	VoteTally Tally() const
	{
		return tally_;
	}
	// The voter's answer: yes, no, or none yet.
	std::optional<bool> Answer(int id) const;
	// Seconds left to vote (rounded up); 0 when not open.
	int SecondsLeft(int64_t now_ms) const;

	// The HUD status line: "VOTE: MAP Q2DM3 - YES 1/2 - 27s" (yes answers of
	// the voters there, seconds left), "VOTE: DUEL: HOONYMODE - YES 1/2 - 27s",
	// "VOTE PASSED"; empty with no vote.
	std::string HudText(int64_t now_ms) const;

  private:
	enum class State
	{
		Idle,
		Open,
		Passed,
	};
	void Recount(const std::vector<int> &voters);
	void Fail(int64_t now_ms);

	VoteConfig config_;
	State state_ = State::Idle;
	VoteProposal proposal_;
	int caller_ = -1;
	int64_t since_ms_ = 0;
	std::map<int, bool> answers_;
	VoteTally tally_;
	std::map<int, int> calls_;            // per player, this map
	std::map<int, int64_t> failed_at_ms_; // per player: when their last vote failed
};
