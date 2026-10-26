// The match flow state machine (docs/design/match-flow.md), as pure logic.
//
// The adapter (core/match_register.cpp) builds a MatchInput from the level and
// clients each frame, calls Update, and carries out the returned actions. With
// MatchConfig::enabled false the machine is LIVE from the start and never acts:
// stock rules run the level, exactly as stock.
#pragma once

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
	RoundCountdown, // between rounds (rounds_on_frag): players held, the arena reset
	Intermission,
};

const char *MatchStateName(MatchState state);

struct MatchConfig
{
	bool enabled = false;              // match_warmup: false = stock (live immediately)
	bool ready_required = true;        // match_ready
	int min_players = 2;               // match_minplayers
	int64_t warmup_time_ms = 0;        // match_warmup_time; 0 = wait for ready
	int64_t countdown_ms = 5000;       // match_countdown
	int overtime = 0;                  // match_overtime: 0 none, -1 sudden death, N minutes
	int64_t timelimit_ms = 0;          // timelimit, from the start of LIVE; 0 = none
	int fraglimit = 0;                 // 0 = none
	bool rounds_on_frag = false;       // match_rounds 1: every score change ends a round (HoonyMode)
	int64_t round_countdown_ms = 3000; // match_round_countdown
};

struct MatchPlayer
{
	int id = 0;           // client slot
	bool playing = false; // false: spectator / not joined
	bool ready = false;
	int score = 0;
};

struct MatchInput
{
	int64_t now_ms = 0;
	std::vector<MatchPlayer> players;
	bool gametype_over = false; // the gametype's own end condition (rounds, captures)
};

enum class MatchActionKind
{
	Announce,   // text to every player (centre print)
	Countdown,  // value = whole seconds left, sent once per second
	ResetArena, // value 0: going LIVE - reset items, zero scores, respawn players
	            // value 1: between rounds - reset items and projectiles only
	RoundHold,  // between rounds: every player out of play, waiting
	RoundStart, // a round begins: every playing client back in at a spawn
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
	MatchState round_return_ = MatchState::Live;    // where RoundCountdown goes back to
	std::vector<std::pair<int, int>> round_scores_; // (id, score) when the round began
};
