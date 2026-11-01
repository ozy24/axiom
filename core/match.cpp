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
	case MatchState::Intermission:
		return "intermission";
	}
	return "?";
}

bool MatchStateInPlay(MatchState state)
{
	return state == MatchState::Live || state == MatchState::Overtime;
}

namespace
{
// How long the HUD names overtime or sudden death as it begins.
constexpr int64_t kOvertimeNoticeMs = 10000;
} // namespace

namespace
{
int PlayingCount(const MatchInput &in)
{
	return (int)std::count_if(in.players.begin(), in.players.end(), [](const MatchPlayer &p) { return p.playing; });
}

int HumansPlaying(const MatchInput &in)
{
	return (int)std::count_if(in.players.begin(), in.players.end(),
	                          [](const MatchPlayer &p) { return p.playing && !p.bot; });
}

// match_minplayers counts everyone playing, bots too (as MuffMode's
// minplayers), so one human and the engine's bot fill can start a match. A
// match also needs a human, so bots alone only ever warm up, unless
// match_allow_no_humans (a bot server, the harness's bot-only tiers): Chris,
// 2026-10-29.
bool EnoughPlayers(const MatchConfig &config, const MatchInput &in)
{
	return PlayingCount(in) >= std::max(1, config.min_players) && (config.allow_no_humans || HumansPlaying(in) > 0);
}

// Enough players, and with teams someone playing on each.
bool Staffed(const MatchConfig &config, const MatchInput &in)
{
	if (!EnoughPlayers(config, in))
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
	// After a map change the bots are back before the humans: the countdown
	// waits for them, or it would start and be cancelled as they arrive unready.
	// Only so long: a client stuck loading mustn't hold the server in warmup.
	if (in.now_ms < config_.join_grace_ms ||
	    (in.now_ms < config_.join_wait_ms &&
	     std::any_of(in.players.begin(), in.players.end(), [](const MatchPlayer &p) { return p.joining; })))
		return false;
	if (config_.warmup_time_ms > 0 && in.now_ms - state_since_ms_ >= config_.warmup_time_ms &&
	    state_ == MatchState::Warmup)
		return true;
	if (!config_.ready_required)
		return true;
	return std::all_of(in.players.begin(), in.players.end(),
	                   [](const MatchPlayer &p) { return !p.playing || p.ready; });
}

namespace
{
std::string Clock(int64_t ms)
{
	return Core_Match_Clock(ms);
}
} // namespace

std::string Core_Match_Clock(int64_t ms)
{
	const int64_t s = ms <= 0 ? 0 : (ms + 999) / 1000;
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%lld:%02lld", (long long)(s / 60), (long long)(s % 60));
	return buf;
}

namespace
{
int SecondsLeft(int64_t total_ms, int64_t elapsed_ms)
{
	const int64_t left = total_ms - elapsed_ms;
	return left <= 0 ? 0 : (int)((left + 999) / 1000);
}

} // namespace

std::string Core_Match_SpokenSeconds(int s)
{
	if (s >= 1 && s <= 10)
		return "world/" + std::to_string(s) + ".wav";
	if (s == 20 || s == 30 || s == 40)
		return "world/" + std::to_string(s) + "sec.wav";
	return "";
}

namespace
{
void Speak(int s, std::vector<MatchAction> &out)
{
	const std::string sound = Core_Match_SpokenSeconds(s);
	if (!sound.empty())
		out.push_back({MatchActionKind::Sound, sound, 0});
}
} // namespace

MatchHudText Core_Match_StockHudText(int64_t now_ms, int64_t timelimit_ms)
{
	if (timelimit_ms <= 0)
		return {};
	return {"", Clock(timelimit_ms - now_ms)};
}

MatchHudText Match::HudText(const MatchInput &in) const
{
	if (!config_.enabled)
		return {};
	// Two texts in places of their own (Chris, 2026-10-27): the status top
	// middle, the clock bottom middle, so the clock always shows. Before the
	// match it shows the match's length, so players see how long it is; Status names are OpenTDM's
	// (g_tdm_core.c), capitalised. The clock shows while the match is being
	// then, timed overtime included, down to the timelimit,
	// else up from LIVE; in sudden death it stays at 0:00.
	const std::string clock = time_end_ms_ ? Clock(time_end_ms_ - in.now_ms) : Clock(in.now_ms - live_since_ms_);
	const std::string length = Clock(config_.timelimit_ms); // 0:00 with no timelimit: it counts up
	switch (state_)
	{
	case MatchState::Warmup:
		// What the start waits for, as Quake Live says it (Chris, 2026-10-27):
		// more players, then someone on each team, then in ready-up how many of
		// the players are ready (bots always are: Chris, 2026-10-29); without
		// ready-up, WARMUP (the countdown is about to start).
		if (!EnoughPlayers(config_, in))
			return {"WAITING FOR PLAYERS", length};
		if (!Staffed(config_, in))
			return {"WAITING FOR TEAMS", length};
		if (config_.ready_required)
		{
			const int ready = (int)std::count_if(in.players.begin(), in.players.end(),
			                                     [](const MatchPlayer &p) { return p.playing && p.ready; });
			return {"READY UP " + std::to_string(ready) + "/" + std::to_string(PlayingCount(in)), length};
		}
		return {"WARMUP", length};
	case MatchState::Countdown:
		return {"COUNTDOWN", length}; // the seconds: the big digits (CountdownSeconds)
	case MatchState::Live:
		return {"", clock};
	case MatchState::Overtime:
	{
		// Named for 10 seconds as it begins (and each new overtime period),
		// then gone: a line that stays is a distraction (Chris, 2026-10-30).
		const bool notice = in.now_ms - overtime_since_ms_ < kOvertimeNoticeMs;
		if (config_.overtime < 0)
			return {notice ? "SUDDEN DEATH" : "", "0:00"}; // the next score ends it
		return {notice ? "OVERTIME" : "", clock};
	}
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
	return 0;
}

void Match::GoLive(int64_t now_ms, std::vector<MatchAction> &out)
{
	state_ = MatchState::Live;
	state_since_ms_ = live_since_ms_ = now_ms;
	time_end_ms_ = config_.timelimit_ms > 0 ? now_ms + config_.timelimit_ms : 0;
	last_warning_s_ = SecondsLeft(config_.timelimit_ms, 0); // nothing spoken for the full time
	out.push_back({MatchActionKind::ResetArena, "", 0});
	out.push_back({MatchActionKind::Announce, "FIGHT!", 0});
	out.push_back({MatchActionKind::Sound, "misc/tele_up.wav", 0});
}

// match_overtime_max: overtime has run that long and it's still tied. The
// match ends a draw (MMREX's g_dm_tie_max_time).
void Match::Draw(std::vector<MatchAction> &out)
{
	out.push_back({MatchActionKind::Announce, "Still tied: the match is a draw", 0});
	End("draw", out);
}

void Match::End(const char *why, std::vector<MatchAction> &out)
{
	state_ = MatchState::Intermission;
	out.push_back({MatchActionKind::EndMatch, why, 0});
	out.push_back({MatchActionKind::Sound, "world/xian1.wav", 0}); // the scoreboard jingle, as MuffMode
}

void Match::ChangeLimits(int64_t timelimit_ms, int fraglimit, int64_t now_ms)
{
	config_.timelimit_ms = timelimit_ms;
	config_.fraglimit = fraglimit;
	if (state_ != MatchState::Live)
		return;
	time_end_ms_ = timelimit_ms > 0 ? live_since_ms_ + timelimit_ms : 0;
	// Nothing spoken for the second it changed in.
	last_warning_s_ = time_end_ms_ ? SecondsLeft(time_end_ms_ - now_ms, 0) : 0;
}

// LIVE with a timelimit: the stock voice at 30 and 20 seconds left, then every
// second from 10, as MuffMode does.
void Match::ToWarmup(int64_t now_ms, std::vector<MatchAction> &out)
{
	state_ = MatchState::Warmup;
	state_since_ms_ = now_ms;
	time_end_ms_ = 0;
	last_countdown_s_ = -1;
	out.push_back({MatchActionKind::ResetArena, "", 0});
}

std::vector<MatchAction> Match::BackToWarmup(int64_t now_ms)
{
	std::vector<MatchAction> out;
	if (config_.enabled && MatchStateInPlay(state_))
		ToWarmup(now_ms, out);
	return out;
}

void Match::TimeWarning(int64_t now_ms, std::vector<MatchAction> &out)
{
	if (!time_end_ms_)
		return;
	const int left_s = SecondsLeft(time_end_ms_ - now_ms, 0);
	if (left_s == last_warning_s_)
		return;
	last_warning_s_ = left_s;
	if (left_s <= 10 || left_s == 20 || left_s == 30)
		Speak(left_s, out);
}

std::vector<MatchAction> Match::Update(const MatchInput &in)
{
	std::vector<MatchAction> out;
	if (!config_.enabled)
		return out; // stock rules own the level

	// Every human has left a match under way: back to warmup, the arena reset
	// and scores zeroed, so the next to arrive don't find a match the bots
	// played. One human left keeps it going, below match_minplayers too
	// (Chris, 2026-10-29). Not with match_allow_no_humans.
	if (!config_.allow_no_humans && MatchStateInPlay(state_) && !HumansPlaying(in))
	{
		ToWarmup(in.now_ms, out);
		return out;
	}

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
			if (left_s <= 10 || left_s % 10 == 0)
				Speak(left_s, out);
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
				state_since_ms_ = overtime_since_ms_ = overtime_start_ms_ = in.now_ms;
				time_end_ms_ = config_.overtime > 0 ? in.now_ms + (int64_t)config_.overtime * 60000 : 0;
				out.push_back({MatchActionKind::Klaxon, "", 0}); // the HUD names the state (Chris, 2026-10-27)
			}
			else
			{
				End("timelimit", out);
			}
			break;
		}
		TimeWarning(in.now_ms, out);
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
		const bool tie_limit = config_.overtime_max_ms > 0 && in.now_ms - overtime_start_ms_ >= config_.overtime_max_ms;
		if (config_.overtime < 0)
		{
			// Sudden death: the first score that breaks the tie ends it.
			if (count < 2 || top != second)
				End("sudden death", out);
			else if (tie_limit)
				Draw(out);
			break;
		}
		if (time_end_ms_ && in.now_ms >= time_end_ms_)
		{
			// A period ends: still tied, another (as MMREX's Duel), unless the
			// tie has lasted match_overtime_max.
			if (count >= 2 && top == second && tie_limit)
			{
				Draw(out);
			}
			else if (count >= 2 && top == second)
			{
				time_end_ms_ = in.now_ms + (int64_t)config_.overtime * 60000;
				overtime_since_ms_ = in.now_ms;                  // named again for the new period
				out.push_back({MatchActionKind::Klaxon, "", 0}); // the HUD names the state (Chris, 2026-10-27)
			}
			else
			{
				End("overtime", out);
			}
			break;
		}
		break;
	}
	case MatchState::Intermission:
		break;
	}
	return out;
}
