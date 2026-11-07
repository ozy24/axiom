// Match timeouts (docs/design/timeouts.md, decision 0025), as pure logic.
//
// A side (a team, or in Duel and FFA a player) may stop a match under way once
// per match: the world stands still for match_timeout seconds, then a
// resume countdown of match_timeout_resume seconds, then play goes on from
// exactly where it stopped. The calling side may end it early (`time-in`),
// which starts the resume countdown; the server console may call and end one
// at any time, outside the allowance.
//
// The adapter (core/timeout_register.cpp) stops the world: while Paused() it
// skips the game frame before level time advances, so every timer, item,
// projectile and the match clock stand still with it. The timeout keeps its
// own time, counted in frames (Tick).
#pragma once

#include "core/match.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct TimeoutConfig
{
	int64_t length_ms = 0;     // match_timeout: 0 = timeouts off
	int64_t resume_ms = 10000; // match_timeout_resume: 0 = play resumes at once
};

enum class TimeoutPhase
{
	None,
	Paused,   // the timeout itself
	Resuming, // the countdown back to play
};

enum class TimeoutError
{
	None,
	Off,        // match_timeout 0
	NotInPlay,  // no match under way: warmup, the countdown, the intermission
	NotPlaying, // a spectator, or on no team
	Used,       // this side's timeout has gone
	Running,    // a timeout is already under way
	NotRunning, // time-in with no timeout
	NotYours,   // time-in by a side that didn't call it
	Resuming,   // time-in once the countdown back to play has begun
};

// What a refusal tells the player; empty for None.
std::string Core_Timeout_ErrorText(TimeoutError e);

class Timeout
{
  public:
	// The server's own caller: its timeouts need no allowance and only the
	// server ends them early.
	static constexpr const char *kServer = "";

	// A side calls a timeout. `side` names it ("red", or a player's key in
	// Duel and FFA); `in_play` and `playing` are the match's and the
	// caller's. With side kServer, the server: no allowance, only `in_play`.
	TimeoutError Call(const std::string &side, bool in_play, bool playing, const TimeoutConfig &config);

	// `time-in`: the side that called it (or the server, for any timeout)
	// ends it early, starting the resume countdown.
	TimeoutError TimeIn(const std::string &side);

	// One game frame of `frame_ms` while Paused(): counts the timeout down.
	// Returns what the adapter must do (the spoken countdown, the announcement
	// that play resumes, FIGHT as it does).
	std::vector<MatchAction> Tick(int64_t frame_ms);

	// A new match (going LIVE, or back to warmup): every side has its timeout
	// again, and any timeout under way is dropped.
	void NewMatch();
	// Drops a timeout under way, keeping the allowance (a new level, every
	// human gone).
	void Cancel();

	bool Paused() const
	{
		return phase_ != TimeoutPhase::None;
	}
	TimeoutPhase Phase() const
	{
		return phase_;
	}
	const std::string &Side() const
	{
		return side_;
	}
	bool Used(const std::string &side) const
	{
		return used_.count(side) != 0;
	}
	int64_t LeftMs() const
	{
		return left_ms_;
	}
	// The HUD's status line: "TIMEOUT 1:45" while paused, "RESUMING" in the
	// countdown back to play; empty otherwise.
	std::string Status() const;
	// The HUD's big digits: whole seconds left in the resume countdown, else 0.
	int CountdownSeconds() const;

  private:
	void BeginResume(std::vector<MatchAction> &out);

	TimeoutPhase phase_ = TimeoutPhase::None;
	std::string side_;
	int64_t left_ms_ = 0;
	int64_t resume_ms_ = 0;
	int last_spoken_s_ = -1;
	std::set<std::string> used_;
};
