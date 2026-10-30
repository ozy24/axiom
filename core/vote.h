// Voting (docs/design/voting.md), as pure logic: what can be voted on, who
// counts, and when a vote passes or fails. No engine access.
//
// Three kinds pick what is played: map, gametype and restart (the current
// map, reloaded). A gametype vote names a ruleset: the gametype's standard one
// or a variant (HoonyMode is Duel's); "ruleset" is accepted as another word
// for it. Four more change one setting each (decision 0015): the time and
// score limits, powerups and friendly fire, from a table compiled in, within
// fixed ranges. A voted setting lasts until the gametype changes.
// Only voters count: humans in the match (in Duel, everyone in the queue).
// The caller's vote is a yes. A vote passes on a majority of the voters there
// now, fails as soon as it can't pass or after the timeout, and runs a few
// seconds after it passes. Each player calls at most a few votes per map and
// waits after one of theirs fails. Votes can be called mid-match (a match
// started with the wrong settings is voted away). Map, gametype, restart and
// powerups reload a map, so the match starts again from warmup; the limits
// and friendly fire apply in place. Two more ready players up in a ready-up
// warmup: readyall (everyone playing) and readyteam (the caller's team, voted
// on by that team alone). In team modes' warmup, shuffle splits the players
// into two new teams at random (core/teams.h's Core_Teams_Shuffle).
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
	Timelimit,
	Scorelimit,
	Powerups,
	FriendlyFire,
	ReadyAll,
	ReadyTeam,
	Shuffle,
};

// "map", "gametype", "restart", "timelimit", "scorelimit", "powerups",
// "friendlyfire", "readyall", "readyteam", "shuffle"; and back, case-insensitively ("ruleset" for gametype;
// "fraglimit" and "capturelimit" for scorelimit).
const char *VoteKindName(VoteKind kind);
std::optional<VoteKind> Core_Vote_ParseKind(std::string_view name);
// Every kind, in the order the menu and usage lines list them.
const std::vector<VoteKind> &Core_Vote_AllKinds();

// The settings now, as the votes on them see them.
struct VoteSettingsNow
{
	int timelimit_min = 0; // timelimit, whole minutes
	int scorelimit = 0;    // fraglimit, or capturelimit in CTF
	bool powerups = true;  // !g_no_items
	bool friendly_fire = false;
	bool teams = false; // TDM, CTF
	bool ctf = false;
	bool ready_up = false; // the match waits for its players to ready up
	bool warmup = false;   // the match flow, before LIVE (warmup or the countdown)
	int team_players = 0;  // players on a team (bots too)
};

// One votable setting (decision 0015). The table is compiled in: admins
// choose which votes are allowed (g_vote_allow), not what they change.
enum class VoteValue
{
	Minutes, // a whole number of minutes in [min, max]
	Count,   // a number in [min, max]; 0 = no limit
	OnOff,
};

struct VoteSetting
{
	VoteKind kind = VoteKind::Timelimit;
	const char *title = ""; // "Time Limit", "Frag Limit", "Capture Limit" (the menu's row)
	VoteValue value = VoteValue::OnOff;
	int min = 0, max = 1;
	std::vector<int> menu;   // the Tab menu's choices (OnOff: on, off)
	bool teams_only = false; // friendly fire
	bool reloads = false;    // items spawn as a map loads: powerups reload it
};

// The setting a kind changes (the score limit is CTF's capture limit there,
// with its own range), or nullopt for map, gametype, restart, the ready votes and shuffle.
std::optional<VoteSetting> Core_Vote_Setting(VoteKind kind, bool ctf);

// A vote's argument as the setting's value: a number in range, or on / off
// (also 1 / 0, yes / no). nullopt if it's neither.
std::optional<int> Core_Vote_SettingValue(const VoteSetting &s, std::string_view arg);
// The value as an argument ("20", "on") and as words ("20 minutes", "none",
// "on").
std::string Core_Vote_SettingArg(const VoteSetting &s, int value);
std::string Core_Vote_SettingText(const VoteSetting &s, int value);
// The setting's value now.
int Core_Vote_SettingNow(const VoteSetting &s, const VoteSettingsNow &now);
// What a setting vote proposes, as the HUD and the broadcasts say it: "time
// limit 20 minutes", "no frag limit", "powerups off".
std::string Core_Vote_SettingLabel(const VoteSetting &s, int value);

struct VoteProposal
{
	VoteKind kind = VoteKind::Restart;
	std::string arg;   // the map or ruleset name; readyteam's team (red, blue); empty for restart, readyall, shuffle
	std::string label; // shown instead of arg if set: "Duel: HoonyMode"

	bool operator==(const VoteProposal &o) const
	{
		return kind == o.kind && arg == o.arg;
	}
};

// "map q2dm3", "gametype Duel: HoonyMode", "restart", "ready red".
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
	VoteSettingsNow now;                // what the setting votes change
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
	BadArg,       // not on the list (map, gametype, ruleset), out of range, or an argument to restart
	AlreadySet,   // a setting vote for the value it has now
	TeamsOnly,    // friendly fire, readyteam or shuffle outside team modes
	WarmupOnly,   // shuffle once the match is live, or with the match flow off
	TooFew,       // shuffle with fewer than three players on teams
	NoReadyUp,    // a ready vote with no ready-up under way
	NoLimit,      // no score limit with no time limit: the match would never end
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
