// The match flow state machine (docs/design/match-flow.md), as pure logic.
//
// The adapter (core/match_register.cpp) builds a MatchInput from the level and
// clients each frame, calls Update, and carries out the returned actions. With
// MatchConfig::enabled false the machine is LIVE from the start and never acts:
// stock rules run the level, exactly as stock.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class MatchState
{
	Warmup,
	Countdown,
	Live,
	Overtime,
	Intermission,
};

const char *MatchStateName(MatchState state);

// The match is being played: live or overtime. Not warmup, the countdown or
// the intermission.
bool MatchStateInPlay(MatchState state);

struct MatchConfig
{
	bool enabled = false;            // match_flow: false = stock (live immediately)
	bool ready_required = true;      // match_ready
	int min_players = 2;             // match_minplayers: playing clients, bots too
	bool allow_no_humans = false;    // match_allow_no_humans: bots may play a match alone
	int64_t warmup_time_ms = 0;      // match_warmup_time; 0 = wait for ready
	int64_t countdown_ms = 10000;    // match_countdown
	int overtime = 0;                // match_overtime: 0 none, -1 sudden death, N minutes
	int64_t timelimit_ms = 0;        // timelimit, from the start of LIVE; 0 = none
	int fraglimit = 0;               // 0 = none
	bool teams = false;              // TDM, CTF: the lead and ties go by MatchInput::team_scores
	bool fraglimit_on_teams = false; // TDM: fraglimit applies to team totals (stock CheckEndTDMLevel)
	int64_t join_grace_ms = 0;       // a new level: no countdown before this (players reloading the map)
	int64_t join_wait_ms = 0;        // ... nor, until this, while a human is still loading it

	bool operator==(const MatchConfig &o) const
	{
		return enabled == o.enabled && ready_required == o.ready_required && min_players == o.min_players &&
		       allow_no_humans == o.allow_no_humans && warmup_time_ms == o.warmup_time_ms &&
		       countdown_ms == o.countdown_ms && overtime == o.overtime && timelimit_ms == o.timelimit_ms &&
		       fraglimit == o.fraglimit && teams == o.teams && fraglimit_on_teams == o.fraglimit_on_teams &&
		       join_grace_ms == o.join_grace_ms && join_wait_ms == o.join_wait_ms;
	}
};

struct MatchPlayer
{
	int id = 0;           // client slot
	bool playing = false; // false: spectator / not joined
	bool ready = false;
	int score = 0;
	int team = 0;         // with MatchConfig::teams: 1 or 2 (0 = on no team)
	bool joining = false; // a human connected but still loading the map: no countdown starts yet
	bool bot = false;     // a match needs a human (unless match_allow_no_humans)
};

struct MatchInput
{
	int64_t now_ms = 0;
	std::vector<MatchPlayer> players;
	bool gametype_over = false;       // the gametype's own end condition (rounds, captures)
	std::string gametype_over_reason; // for the record; empty = "gametype"
	std::array<int, 2> team_scores{}; // with MatchConfig::teams: team 1, team 2 (frags or captures)
};

struct MatchHudText
{
	std::string status;
	std::string clock;
};

// The HUD with the match flow off (stock's rules): no status, and the clock
// counts down stock's timelimit from the level's start (CheckDMRules), 0:00
// once it's up. No clock with no timelimit.
MatchHudText Core_Match_StockHudText(int64_t now_ms, int64_t timelimit_ms);

enum class MatchActionKind
{
	Announce,   // text to every player (centre print)
	Klaxon,     // the stock klaxon, heard everywhere: overtime and sudden death begin
	Sound,      // text = a stock sound, heard everywhere: the spoken countdown, FIGHT, match end
	Countdown,  // value = whole seconds left, sent once per second
	ResetArena, // going LIVE (or back to warmup): reset items, zero scores, respawn players
	EndMatch,   // go to intermission
};

struct MatchAction
{
	MatchActionKind kind;
	std::string text;
	int value = 0;

	bool operator==(const MatchAction &o) const
	{
		return kind == o.kind && text == o.text && value == o.value;
	}
};

class Match
{
  public:
	explicit Match(MatchConfig config = {});

	// Map load: WARMUP if enabled, otherwise LIVE (stock).
	void Begin(int64_t now_ms);

	// Advances the machine; returns what the adapter must do this frame.
	std::vector<MatchAction> Update(const MatchInput &in);

	MatchState State() const
	{
		return state_;
	}
	const MatchConfig &Config() const
	{
		return config_;
	}
	// When LIVE started (timelimit counts from here).
	int64_t LiveSinceMs() const
	{
		return live_since_ms_;
	}
	// Whether the match flow, not stock CheckDMRules, decides when the level ends.
	bool OwnsRules() const
	{
		return config_.enabled;
	}
	// Whether frags, suicides and captures count. Not before LIVE: warmup and
	// the countdown are free play, and the adapter holds every score at zero.
	bool ScoresCount() const
	{
		return !config_.enabled || (state_ != MatchState::Warmup && state_ != MatchState::Countdown);
	}
	// Whether playing clients may move but not fire: the match countdown.
	bool HoldFire() const
	{
		return config_.enabled && state_ == MatchState::Countdown;
	}
	// The HUD's match status and clock (docs/design/hud.md). The status, top
	// middle: OpenTDM's names (WAITING FOR PLAYERS / TEAMS, WARMUP, COUNTDOWN,
	// OVERTIME, SUDDEN DEATH, MATCH END), none in normal live
	// play. The clock, bottom middle, always: the match's length before it
	// starts, then the time left (or up from LIVE with no timelimit), 0:00 in
	// sudden death. Both empty with the match flow off.
	MatchHudText HudText(const MatchInput &in) const;
	// The HUD's big countdown digits: whole seconds left in the match
	// countdown, else 0 (hidden).
	int CountdownSeconds(int64_t now_ms) const;

	// A vote changed the time or frag limit (decision 0015). Before LIVE the
	// match just starts with them. In regulation time (LIVE) the end moves,
	// still counted from LIVE: a limit already passed ends the match at the
	// next update, as stock's CheckDMRules would. Overtime keeps its own end.
	void ChangeLimits(int64_t timelimit_ms, int fraglimit, int64_t now_ms);

	// When the HUD clock reaches zero (level ms), or 0 for no clock: none in
	// warmup, the countdown or sudden death; the timelimit from LIVE, then
	// timed overtime. Only meaningful when OwnsRules().
	int64_t ClockEndMs() const
	{
		return MatchStateInPlay(state_) ? time_end_ms_ : 0;
	}

  private:
	bool CanStart(const MatchInput &in) const;
	void GoLive(int64_t now_ms, std::vector<MatchAction> &out);
	void End(const char *why, std::vector<MatchAction> &out);
	void TimeWarning(int64_t now_ms, std::vector<MatchAction> &out);

	MatchConfig config_;
	MatchState state_ = MatchState::Live;
	int64_t state_since_ms_ = 0;
	int64_t live_since_ms_ = 0;
	int64_t time_end_ms_ = 0; // LIVE/OVERTIME deadline (0 = none)
	int last_countdown_s_ = -1;
	int last_warning_s_ = 0; // LIVE: the seconds left last spoken for (timelimit warnings)
};
