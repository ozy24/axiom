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
#include <utility>
#include <vector>

enum class MatchState
{
	Warmup,
	Countdown,
	Live,
	Overtime,
	RoundOver,      // a frag ended the round: the winner plays on, the dead stay dead
	RoundCountdown, // then everyone respawned, frozen, counting down to the next round
	Intermission,
};

const char *MatchStateName(MatchState state);

struct MatchPlayer;

// The one playing client other than `self`, or -1 if there isn't exactly one
// (match_suicide_point gives that opponent a suicide's point).
int Core_SuicideOpponent(const std::vector<MatchPlayer> &players, int self);

struct MatchConfig
{
	bool enabled = false;              // match_warmup: false = stock (live immediately)
	bool ready_required = true;        // match_ready
	int min_players = 2;               // match_minplayers
	int64_t warmup_time_ms = 0;        // match_warmup_time; 0 = wait for ready
	int64_t countdown_ms = 10000;      // match_countdown
	int overtime = 0;                  // match_overtime: 0 none, -1 sudden death, N minutes
	int64_t timelimit_ms = 0;          // timelimit, from the start of LIVE; 0 = none
	int fraglimit = 0;                 // 0 = none
	bool rounds_on_frag = false;       // match_rounds 1: every score change ends a round (HoonyMode)
	int64_t round_pause_ms = 2000;     // match_round_pause: after the frag, before the respawn
	int64_t round_countdown_ms = 5000; // match_round_countdown: frozen at the spawns
	bool teams = false;                // TDM, CTF: the lead and ties go by MatchInput::team_scores
	bool fraglimit_on_teams = false;   // TDM: fraglimit applies to team totals (stock CheckEndTDMLevel)
};

struct MatchPlayer
{
	int id = 0;           // client slot
	bool playing = false; // false: spectator / not joined
	bool ready = false;
	int score = 0;
	int team = 0; // with MatchConfig::teams: 1 or 2 (0 = on no team)
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

enum class MatchActionKind
{
	Announce,     // text to every player (centre print)
	Klaxon,       // the stock klaxon, heard everywhere: overtime and sudden death begin
	Countdown,    // value = whole seconds left, sent once per second
	ResetArena,   // value 0: going LIVE - reset items, zero scores, respawn players
	              // value 1: between rounds - reset items and projectiles only
	RoundHold,    // the round is over: dead players stay dead (no respawn) until RoundRespawn
	RoundRespawn, // every playing client back in at a spawn, frozen until RoundStart
	RoundStart,   // the round begins: everyone released
	EndMatch,     // go to intermission
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
	// When the HUD clock reaches zero (level ms), or 0 for no clock: none in
	// warmup, the countdown or sudden death; the timelimit from LIVE, then
	// timed overtime. Only meaningful when OwnsRules().
	// Whether playing clients are held in place (the round countdown): they can
	// look around but not move or fire.
	bool Frozen() const
	{
		return config_.enabled && state_ == MatchState::RoundCountdown;
	}
	// Whether playing clients may move but not fire: the match countdown.
	bool HoldFire() const
	{
		return config_.enabled && state_ == MatchState::Countdown;
	}
	// The round being played (rounds_on_frag): 1 from LIVE, +1 each new round.
	int Round() const
	{
		return round_;
	}
	// The HUD's match status and clock (docs/design/hud.md): the status above
	// (OpenTDM's names: WARMUP, COUNTDOWN, OVERTIME, MATCH END - none in normal
	// live play; ROUND n in rounds; SUDDEN DEATH over a clock held at 0:00), the line below ("9:42", "1/2
	// READY" in warmup). Both empty with the match flow off.
	MatchHudText HudText(const MatchInput &in) const;
	// The HUD's big countdown digits: whole seconds left in the match or round
	// countdown, else 0 (hidden).
	int CountdownSeconds(int64_t now_ms) const;

	int64_t ClockEndMs() const
	{
		const bool timed = state_ == MatchState::Live || state_ == MatchState::Overtime ||
		                   state_ == MatchState::RoundOver || state_ == MatchState::RoundCountdown;
		return timed ? time_end_ms_ : 0;
	}

  private:
	bool CanStart(const MatchInput &in) const;
	bool ScoreChanged(const MatchInput &in) const;
	bool RoundBreak(const MatchInput &in, MatchState from, std::vector<MatchAction> &out);
	void Baseline(const MatchInput &in);
	void GoLive(int64_t now_ms, std::vector<MatchAction> &out);
	void End(const char *why, std::vector<MatchAction> &out);

	MatchConfig config_;
	MatchState state_ = MatchState::Live;
	int64_t state_since_ms_ = 0;
	int64_t live_since_ms_ = 0;
	int64_t time_end_ms_ = 0; // LIVE/OVERTIME deadline (0 = none)
	int last_countdown_s_ = -1;
	MatchState round_return_ = MatchState::Live; // where RoundCountdown goes back to
	int round_ = 0;
	std::vector<std::pair<int, int>> round_scores_; // (id, score) when the round began
};
