// Pure part of match timeouts (core/timeout.h). No engine access.
#include "core/timeout.h"

std::string Core_Timeout_ErrorText(TimeoutError e)
{
	switch (e)
	{
	case TimeoutError::None:
		return "";
	case TimeoutError::Off:
		return "Timeouts are off on this server.";
	case TimeoutError::NotInPlay:
		return "Timeouts are only for a match under way.";
	case TimeoutError::NotPlaying:
		return "Only players in the match can call a timeout.";
	case TimeoutError::Used:
		return "Your timeout for this match has been used.";
	case TimeoutError::Running:
		return "A timeout is already under way.";
	case TimeoutError::NotRunning:
		return "There is no timeout to end.";
	case TimeoutError::NotYours:
		return "Only the side that called the timeout can end it.";
	case TimeoutError::Resuming:
		return "The match is already resuming.";
	}
	return "";
}

TimeoutError Timeout::Call(const std::string &side, bool in_play, bool playing, const TimeoutConfig &config)
{
	const bool server = side == kServer;
	if (config.length_ms <= 0 && !server)
		return TimeoutError::Off;
	if (!in_play)
		return TimeoutError::NotInPlay;
	if (!server && !playing)
		return TimeoutError::NotPlaying;
	if (Paused())
		return TimeoutError::Running;
	if (!server && Used(side))
		return TimeoutError::Used;
	if (!server)
		used_.insert(side);
	phase_ = TimeoutPhase::Paused;
	side_ = side;
	// The server's own runs for match_timeout too, or 2 minutes with
	// timeouts off; it may end it whenever it likes.
	left_ms_ = config.length_ms > 0 ? config.length_ms : 120000;
	resume_ms_ = config.resume_ms > 0 ? config.resume_ms : 0;
	last_spoken_s_ = -1;
	return TimeoutError::None;
}

TimeoutError Timeout::TimeIn(const std::string &side)
{
	if (phase_ == TimeoutPhase::None)
		return TimeoutError::NotRunning;
	if (phase_ == TimeoutPhase::Resuming)
		return TimeoutError::Resuming;
	if (side != kServer && side != side_)
		return TimeoutError::NotYours;
	left_ms_ = 0; // the next Tick begins the countdown back to play
	return TimeoutError::None;
}

void Timeout::BeginResume(std::vector<MatchAction> &out)
{
	if (resume_ms_ <= 0)
	{
		phase_ = TimeoutPhase::None;
		out.push_back({MatchActionKind::Announce, "FIGHT!", 0});
		out.push_back({MatchActionKind::Sound, "misc/tele_up.wav", 0});
		return;
	}
	phase_ = TimeoutPhase::Resuming;
	left_ms_ = resume_ms_;
	last_spoken_s_ = -1;
	out.push_back({MatchActionKind::Announce, "Match resumes", 0});
}

std::vector<MatchAction> Timeout::Tick(int64_t frame_ms)
{
	std::vector<MatchAction> out;
	switch (phase_)
	{
	case TimeoutPhase::None:
		break;
	case TimeoutPhase::Paused:
		if (left_ms_ <= 0)
		{
			BeginResume(out);
			break;
		}
		left_ms_ -= frame_ms;
		break;
	case TimeoutPhase::Resuming:
	{
		const int s = CountdownSeconds();
		if (s != last_spoken_s_)
		{
			last_spoken_s_ = s;
			const std::string sound = s <= 10 || s % 10 == 0 ? Core_Match_SpokenSeconds(s) : "";
			if (!sound.empty())
				out.push_back({MatchActionKind::Sound, sound, 0});
		}
		left_ms_ -= frame_ms;
		if (left_ms_ <= 0)
		{
			phase_ = TimeoutPhase::None;
			out.push_back({MatchActionKind::Announce, "FIGHT!", 0});
			out.push_back({MatchActionKind::Sound, "misc/tele_up.wav", 0});
		}
		break;
	}
	}
	return out;
}

void Timeout::NewMatch()
{
	Cancel();
	used_.clear();
}

void Timeout::Cancel()
{
	phase_ = TimeoutPhase::None;
	side_.clear();
	left_ms_ = 0;
	last_spoken_s_ = -1;
}

std::string Timeout::Status() const
{
	switch (phase_)
	{
	case TimeoutPhase::None:
		return "";
	case TimeoutPhase::Paused:
		return "TIMEOUT " + Core_Match_Clock(left_ms_);
	case TimeoutPhase::Resuming:
		return "RESUMING";
	}
	return "";
}

int Timeout::CountdownSeconds() const
{
	if (phase_ != TimeoutPhase::Resuming || left_ms_ <= 0)
		return 0;
	return (int)((left_ms_ + 999) / 1000);
}
