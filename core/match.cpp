// Pure part of the match flow: the state machine. No engine access.
#include "core/match.h"

#include <algorithm>
#include <cstdio>
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
	case MatchState::RoundOver:
		return "round over";
	case MatchState::RoundCountdown:
		return "round countdown";
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

// Enough players, and with teams someone playing on each.
bool Staffed(const MatchConfig &config, const MatchInput &in)
{
	if (PlayingCount(in) < std::max(1, config.min_players))
		return false;
	if (!config.teams)
		return true;
	const auto on = [&](int team)
	{
		return std::any_of(in.players.begin(), in.players.end(),
		                   [&](const MatchPlayer &p) { return p.playing && p.team == team; });
	};
	return on(1) && on(2);
}

// Highest and second-highest score among playing clients, or between the two
// teams when `teams`.
void TopTwo(const MatchInput &in, bool teams, int &top, int &second, int &count)
{
	std::vector<int> scores;
	if (teams)
		scores.assign(in.team_scores.begin(), in.team_scores.end());
	else
	{
		for (const MatchPlayer &p : in.players)
		{
			if (p.playing)
				scores.push_back(p.score);
		}
	}
	std::sort(scores.begin(), scores.end(), std::greater<int>());
	count = (int)scores.size();
	top = count > 0 ? scores[0] : 0;
	second = count > 1 ? scores[1] : 0;
}
const char *GametypeReason(const MatchInput &in)
{
	return in.gametype_over_reason.empty() ? "gametype" : in.gametype_over_reason.c_str();
}
} // namespace

int Core_SuicideOpponent(const std::vector<MatchPlayer> &players, int self)
{
	int found = -1;
	for (const MatchPlayer &p : players)
	{
		if (!p.playing || p.id == self)
			continue;
		if (found != -1)
			return -1; // more than one opponent: whose point would it be?
		found = p.id;
	}
	return found;
}

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
	if (!Staffed(config_, in))
		return false;
	if (config_.warmup_time_ms > 0 && in.now_ms - state_since_ms_ >= config_.warmup_time_ms &&
	    state_ == MatchState::Warmup)
		return true;
	if (!config_.ready_required)
		return true;
	return std::all_of(in.players.begin(), in.players.end(),
	                   [](const MatchPlayer &p) { return !p.playing || p.ready; });
}

bool Match::ScoreChanged(const MatchInput &in) const
{
	for (const MatchPlayer &p : in.players)
	{
		if (!p.playing)
			continue;
		for (const auto &[id, score] : round_scores_)
		{
			if (id == p.id && score != p.score)
				return true;
		}
	}
	return false;
}

void Match::Baseline(const MatchInput &in)
{
	round_scores_.clear();
	for (const MatchPlayer &p : in.players)
		round_scores_.emplace_back(p.id, p.score);
}

// HoonyMode: a score changed during a round, and the match isn't over. The
// round is over: the winner plays on while the dead stay dead; after the
// pause everyone respawns, frozen, and counts down to the next round.
bool Match::RoundBreak(const MatchInput &in, MatchState from, std::vector<MatchAction> &out)
{
	if (!config_.rounds_on_frag)
		return false;
	if (round_scores_.empty())
	{
		Baseline(in); // the first frame of LIVE: scores were just zeroed
		return false;
	}
	if (!ScoreChanged(in))
		return false;
	round_return_ = from;
	state_ = MatchState::RoundOver;
	state_since_ms_ = in.now_ms;
	last_countdown_s_ = -1;
	out.push_back({MatchActionKind::RoundHold, "", 0});
	return true;
}

namespace
{
// "M:SS" for a whole number of seconds, rounded up (a clock showing 0:00
// has run out).
std::string Clock(int64_t ms)
{
	const int64_t s = ms <= 0 ? 0 : (ms + 999) / 1000;
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%lld:%02lld", (long long)(s / 60), (long long)(s % 60));
	return buf;
}

int SecondsLeft(int64_t total_ms, int64_t elapsed_ms)
{
	const int64_t left = total_ms - elapsed_ms;
	return left <= 0 ? 0 : (int)((left + 999) / 1000);
}
} // namespace

MatchHudText Match::HudText(const MatchInput &in) const
{
	if (!config_.enabled)
		return {};
	// Status names are OpenTDM's (g_tdm_core.c), capitalised - WARMUP,
	// COUNTDOWN, OVERTIME, MATCH END - but none during normal live play, where
	// the running clock says it (Chris, 2026-10-27). The match clock, while one
	// runs: down to the timelimit, else up from LIVE.
	const std::string clock = time_end_ms_ ? Clock(time_end_ms_ - in.now_ms) : Clock(in.now_ms - live_since_ms_);
	const std::string round = config_.rounds_on_frag ? "ROUND " + std::to_string(round_) : "";
	switch (state_)
	{
	case MatchState::Warmup:
	{
		if (!config_.ready_required)
			return {"WARMUP", ""};
		int playing = 0, ready = 0;
		for (const MatchPlayer &p : in.players)
		{
			playing += p.playing;
			ready += p.playing && p.ready;
		}
		// The ready count goes on the clock's line, free until the match starts.
		return {"WARMUP", std::to_string(ready) + "/" + std::to_string(playing) + " READY"};
	}
	case MatchState::Countdown:
		return {"COUNTDOWN", ""}; // the seconds: the big digits (CountdownSeconds)
	case MatchState::Live:
		return {round, clock}; // just the clock: a running clock means the match is on
	case MatchState::Overtime:
		if (config_.overtime < 0)
			return {"SUDDEN DEATH", ""}; // no clock: the next score ends it
		return {"OVERTIME", clock};
	case MatchState::RoundOver:
		return {"ROUND OVER", clock};
	case MatchState::RoundCountdown:
		return {"COUNTDOWN", clock};
	case MatchState::Intermission:
		return {"MATCH END", ""};
	}
	return {};
}

int Match::CountdownSeconds(int64_t now_ms) const
{
	if (!config_.enabled)
		return 0;
	if (state_ == MatchState::Countdown)
		return SecondsLeft(config_.countdown_ms, now_ms - state_since_ms_);
	if (state_ == MatchState::RoundCountdown)
		return SecondsLeft(config_.round_countdown_ms, now_ms - state_since_ms_);
	return 0;
}

void Match::GoLive(int64_t now_ms, std::vector<MatchAction> &out)
{
	round_ = 1;
	state_ = MatchState::Live;
	state_since_ms_ = live_since_ms_ = now_ms;
	time_end_ms_ = config_.timelimit_ms > 0 ? now_ms + config_.timelimit_ms : 0;
	out.push_back({MatchActionKind::ResetArena, "", 0});
	out.push_back({MatchActionKind::Announce, "FIGHT!", 0});
	round_scores_.clear(); // scores are zeroed by the reset; filled on the next Update
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
		const bool still_ok =
		    Staffed(config_, in) && (timer_start || !config_.ready_required ||
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
			End(GametypeReason(in), out);
			break;
		}
		int top, second, count;
		TopTwo(in, config_.teams, top, second, count);
		if (config_.fraglimit > 0)
		{
			int frag_top, frag_second, frag_count;
			TopTwo(in, config_.teams && config_.fraglimit_on_teams, frag_top, frag_second, frag_count);
			if (frag_count > 0 && frag_top >= config_.fraglimit)
			{
				End("fraglimit", out);
				break;
			}
		}
		if (time_end_ms_ && in.now_ms >= time_end_ms_)
		{
			const bool tied = count >= 2 && top == second;
			if (tied && config_.overtime != 0)
			{
				state_ = MatchState::Overtime;
				state_since_ms_ = in.now_ms;
				time_end_ms_ = config_.overtime > 0 ? in.now_ms + (int64_t)config_.overtime * 60000 : 0;
				out.push_back({MatchActionKind::Klaxon, "", 0}); // the HUD names the state (Chris, 2026-10-27)
			}
			else
			{
				End("timelimit", out);
			}
			break;
		}
		if (RoundBreak(in, MatchState::Live, out))
			break;
		break;
	}
	case MatchState::Overtime:
	{
		if (in.gametype_over)
		{
			End(GametypeReason(in), out);
			break;
		}
		int top, second, count;
		TopTwo(in, config_.teams, top, second, count);
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
				out.push_back({MatchActionKind::Klaxon, "", 0}); // the HUD names the state (Chris, 2026-10-27)
			}
			else
			{
				End("overtime", out);
			}
			break;
		}
		RoundBreak(in, MatchState::Overtime, out);
		break;
	}
	case MatchState::RoundOver:
	{
		if (in.gametype_over)
		{
			End(GametypeReason(in), out);
			break;
		}
		const bool out_of_time = round_return_ == MatchState::Live && time_end_ms_ && in.now_ms >= time_end_ms_;
		if (out_of_time || in.now_ms - state_since_ms_ >= config_.round_pause_ms)
		{
			// Items back, everyone back in at a spawn.
			out.push_back({MatchActionKind::ResetArena, "", 1});
			out.push_back({MatchActionKind::RoundRespawn, "", 0});
			if (out_of_time)
			{
				// Time ran out: decide as LIVE would, on the next frame.
				state_ = MatchState::Live;
				out.push_back({MatchActionKind::RoundStart, "", 0});
				break;
			}
			state_ = MatchState::RoundCountdown;
			state_since_ms_ = in.now_ms;
			last_countdown_s_ = -1;
		}
		break;
	}
	case MatchState::RoundCountdown:
	{
		if (in.gametype_over)
		{
			End(GametypeReason(in), out);
			break;
		}
		if (round_return_ == MatchState::Live && time_end_ms_ && in.now_ms >= time_end_ms_)
		{
			// Time ran out between rounds: decide as LIVE would, on the next frame.
			state_ = MatchState::Live;
			out.push_back({MatchActionKind::RoundStart, "", 0});
			break;
		}
		const int64_t left_ms = config_.round_countdown_ms - (in.now_ms - state_since_ms_);
		if (left_ms <= 0)
		{
			state_ = round_return_;
			state_since_ms_ = in.now_ms;
			++round_;
			Baseline(in);
			out.push_back({MatchActionKind::RoundStart, "", 0});
			out.push_back({MatchActionKind::Announce, "FIGHT!", 0});
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
	case MatchState::Intermission:
		break;
	}
	return out;
}
