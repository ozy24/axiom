// Pure part of the match flow: the state machine. No engine access.
#include "core/match.h"

#include <algorithm>
#include <functional>

const char *MatchStateName(MatchState state)
{
	switch (state)
	{
	case MatchState::Warmup:
		return "warmup";
	case MatchState::Countdown:
		return "countdown";
	case MatchState::Live:
		return "live";
	case MatchState::Overtime:
		return "overtime";
	case MatchState::Intermission:
		return "intermission";
	}
	return "?";
}

namespace
{
int PlayingCount(const MatchInput &in)
{
	return (int)std::count_if(in.players.begin(), in.players.end(), [](const MatchPlayer &p) { return p.playing; });
}

// Highest and second-highest score among playing clients.
void TopTwo(const MatchInput &in, int &top, int &second, int &count)
{
	std::vector<int> scores;
	for (const MatchPlayer &p : in.players)
	{
		if (p.playing)
			scores.push_back(p.score);
	}
	std::sort(scores.begin(), scores.end(), std::greater<int>());
	count = (int)scores.size();
	top = count > 0 ? scores[0] : 0;
	second = count > 1 ? scores[1] : 0;
}
} // namespace

Match::Match(MatchConfig config) : config_(config) {}

void Match::Begin(int64_t now_ms)
{
	state_since_ms_ = now_ms;
	last_countdown_s_ = -1;
	if (config_.enabled)
	{
		state_ = MatchState::Warmup;
	}
	else
	{
		// Stock: live from the first frame, stock rules end the level.
		state_ = MatchState::Live;
		live_since_ms_ = now_ms;
	}
}

bool Match::CanStart(const MatchInput &in) const
{
	if (PlayingCount(in) < std::max(1, config_.min_players))
		return false;
	if (config_.warmup_time_ms > 0 && in.now_ms - state_since_ms_ >= config_.warmup_time_ms &&
	    state_ == MatchState::Warmup)
		return true;
	if (!config_.ready_required)
		return true;
	return std::all_of(in.players.begin(), in.players.end(),
	                   [](const MatchPlayer &p) { return !p.playing || p.ready; });
}

void Match::GoLive(int64_t now_ms, std::vector<MatchAction> &out)
{
	state_ = MatchState::Live;
	state_since_ms_ = live_since_ms_ = now_ms;
	time_end_ms_ = config_.timelimit_ms > 0 ? now_ms + config_.timelimit_ms : 0;
	out.push_back({MatchActionKind::ResetArena, "", 0});
	out.push_back({MatchActionKind::Announce, "FIGHT!", 0});
}

void Match::End(const char *why, std::vector<MatchAction> &out)
{
	state_ = MatchState::Intermission;
	out.push_back({MatchActionKind::EndMatch, why, 0});
}

std::vector<MatchAction> Match::Update(const MatchInput &in)
{
	std::vector<MatchAction> out;
	if (!config_.enabled)
		return out; // stock rules own the level

	switch (state_)
	{
	case MatchState::Warmup:
		if (CanStart(in))
		{
			state_ = MatchState::Countdown;
			state_since_ms_ = in.now_ms;
			last_countdown_s_ = -1;
			out.push_back({MatchActionKind::Announce, "Match starting", 0});
		}
		else
		{
			break;
		}
		[[fallthrough]];
	case MatchState::Countdown:
	{
		// Cancel if the start condition no longer holds (someone un-readied or
		// left). A warmup-timer start isn't re-checked against ready.
		const bool timer_start = config_.warmup_time_ms > 0;
		const bool still_ok = PlayingCount(in) >= std::max(1, config_.min_players) &&
		                      (timer_start || !config_.ready_required ||
		                       std::all_of(in.players.begin(), in.players.end(),
		                                   [](const MatchPlayer &p) { return !p.playing || p.ready; }));
		if (!still_ok)
		{
			state_ = MatchState::Warmup;
			state_since_ms_ = in.now_ms;
			out.push_back({MatchActionKind::Announce, "Countdown cancelled", 0});
			break;
		}
		const int64_t left_ms = config_.countdown_ms - (in.now_ms - state_since_ms_);
		if (left_ms <= 0)
		{
			GoLive(in.now_ms, out);
			break;
		}
		const int left_s = (int)((left_ms + 999) / 1000);
		if (left_s != last_countdown_s_)
		{
			last_countdown_s_ = left_s;
			out.push_back({MatchActionKind::Countdown, "", left_s});
		}
		break;
	}
	case MatchState::Live:
	{
		if (in.gametype_over)
		{
			End("gametype", out);
			break;
		}
		int top, second, count;
		TopTwo(in, top, second, count);
		if (config_.fraglimit > 0 && count > 0 && top >= config_.fraglimit)
		{
			End("fraglimit", out);
			break;
		}
		if (time_end_ms_ && in.now_ms >= time_end_ms_)
		{
			const bool tied = count >= 2 && top == second;
			if (tied && config_.overtime != 0)
			{
				state_ = MatchState::Overtime;
				state_since_ms_ = in.now_ms;
				time_end_ms_ = config_.overtime > 0 ? in.now_ms + (int64_t)config_.overtime * 60000 : 0;
				out.push_back({MatchActionKind::Announce, config_.overtime < 0 ? "Sudden death!" : "Overtime!", 0});
			}
			else
			{
				End("timelimit", out);
			}
		}
		break;
	}
	case MatchState::Overtime:
	{
		if (in.gametype_over)
		{
			End("gametype", out);
			break;
		}
		int top, second, count;
		TopTwo(in, top, second, count);
		if (config_.overtime < 0)
		{
			// Sudden death: the first score that breaks the tie ends it.
			if (count < 2 || top != second)
				End("sudden death", out);
			break;
		}
		if (time_end_ms_ && in.now_ms >= time_end_ms_)
		{
			if (count >= 2 && top == second)
			{
				time_end_ms_ = in.now_ms + (int64_t)config_.overtime * 60000;
				out.push_back({MatchActionKind::Announce, "Overtime!", 0});
			}
			else
			{
				End("overtime", out);
			}
		}
		break;
	}
	case MatchState::Intermission:
		break;
	}
	return out;
}
