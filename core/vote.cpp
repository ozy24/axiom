// Pure part of voting (core/vote.h). No engine access.
#include "core/vote.h"

#include "core/maps.h"

#include <algorithm>
#include <cctype>

namespace
{
std::string Lower(std::string_view s)
{
	std::string out(s);
	for (char &c : out)
		c = (char)std::tolower((unsigned char)c);
	return out;
}

std::string Upper(std::string_view s)
{
	std::string out(s);
	for (char &c : out)
		c = (char)std::toupper((unsigned char)c);
	return out;
}

bool Contains(const std::vector<std::string> &list, std::string_view s)
{
	return std::find(list.begin(), list.end(), s) != list.end();
}

std::string Join(const std::vector<std::string> &list)
{
	std::string out;
	for (const std::string &s : list)
		out += (out.empty() ? "" : " ") + s;
	return out;
}

bool IsVoter(const std::vector<int> &voters, int id)
{
	return std::find(voters.begin(), voters.end(), id) != voters.end();
}
} // namespace

const char *VoteKindName(VoteKind kind)
{
	switch (kind)
	{
	case VoteKind::Map:
		return "map";
	case VoteKind::Gametype:
		return "gametype";
	case VoteKind::Restart:
		return "restart";
	case VoteKind::Timelimit:
		return "timelimit";
	case VoteKind::Scorelimit:
		return "scorelimit";
	case VoteKind::Powerups:
		return "powerups";
	case VoteKind::FriendlyFire:
		return "friendlyfire";
	case VoteKind::ReadyAll:
		return "readyall";
	case VoteKind::ReadyTeam:
		return "readyteam";
	case VoteKind::Shuffle:
		return "shuffle";
	}
	return "?";
}

const std::vector<VoteKind> &Core_Vote_AllKinds()
{
	static const std::vector<VoteKind> kinds{
	    VoteKind::Map,      VoteKind::Gametype,     VoteKind::Restart,  VoteKind::Timelimit, VoteKind::Scorelimit,
	    VoteKind::Powerups, VoteKind::FriendlyFire, VoteKind::ReadyAll, VoteKind::ReadyTeam, VoteKind::Shuffle};
	return kinds;
}

std::optional<VoteKind> Core_Vote_ParseKind(std::string_view name)
{
	const std::string n = Lower(name);
	for (VoteKind k : Core_Vote_AllKinds())
		if (n == VoteKindName(k))
			return k;
	if (n == "ruleset")
		return VoteKind::Gametype;
	if (n == "fraglimit" || n == "capturelimit")
		return VoteKind::Scorelimit;
	return std::nullopt;
}

std::optional<VoteSetting> Core_Vote_Setting(VoteKind kind, bool ctf)
{
	// The table (decision 0015). Ranges are fixed: wide enough for any
	// sensible match, narrow enough that a vote can't make a silly one.
	switch (kind)
	{
	case VoteKind::Timelimit:
		return VoteSetting{kind, "Time Limit", VoteValue::Minutes, 1, 60, {5, 10, 15, 20, 30}, false, false};
	case VoteKind::Scorelimit:
		if (ctf)
			return VoteSetting{kind, "Capture Limit", VoteValue::Count, 0, 20, {0, 3, 5, 8, 10}, false, false};
		return VoteSetting{kind, "Frag Limit", VoteValue::Count, 0, 100, {0, 10, 20, 30, 50}, false, false};
	case VoteKind::Powerups:
		return VoteSetting{kind, "Powerups", VoteValue::OnOff, 0, 1, {1, 0}, false, true};
	case VoteKind::FriendlyFire:
		return VoteSetting{kind, "Friendly Fire", VoteValue::OnOff, 0, 1, {1, 0}, true, false};
	case VoteKind::Map:
	case VoteKind::Gametype:
	case VoteKind::Restart:
	case VoteKind::ReadyAll:
	case VoteKind::ReadyTeam:
	case VoteKind::Shuffle:
		break;
	}
	return std::nullopt;
}

std::optional<int> Core_Vote_SettingValue(const VoteSetting &s, std::string_view arg)
{
	const std::string a = Lower(arg);
	if (s.value == VoteValue::OnOff)
	{
		if (a == "on" || a == "1" || a == "yes")
			return 1;
		if (a == "off" || a == "0" || a == "no")
			return 0;
		return std::nullopt;
	}
	if (a.empty() || a.size() > 6 || !std::all_of(a.begin(), a.end(), [](char c) { return c >= '0' && c <= '9'; }))
		return std::nullopt;
	const int v = std::stoi(a);
	if (v < s.min || v > s.max)
		return std::nullopt;
	return v;
}

std::string Core_Vote_SettingArg(const VoteSetting &s, int value)
{
	if (s.value == VoteValue::OnOff)
		return value ? "on" : "off";
	return std::to_string(value);
}

std::string Core_Vote_SettingText(const VoteSetting &s, int value)
{
	switch (s.value)
	{
	case VoteValue::OnOff:
		return value ? "on" : "off";
	case VoteValue::Minutes:
		return std::to_string(value) + (value == 1 ? " minute" : " minutes");
	case VoteValue::Count:
		return value ? std::to_string(value) : "none";
	}
	return {};
}

int Core_Vote_SettingNow(const VoteSetting &s, const VoteSettingsNow &now)
{
	switch (s.kind)
	{
	case VoteKind::Timelimit:
		return now.timelimit_min;
	case VoteKind::Scorelimit:
		return now.scorelimit;
	case VoteKind::Powerups:
		return now.powerups ? 1 : 0;
	case VoteKind::FriendlyFire:
		return now.friendly_fire ? 1 : 0;
	case VoteKind::Map:
	case VoteKind::Gametype:
	case VoteKind::Restart:
	case VoteKind::ReadyAll:
	case VoteKind::ReadyTeam:
	case VoteKind::Shuffle:
		break;
	}
	return 0;
}

std::string Core_Vote_SettingLabel(const VoteSetting &s, int value)
{
	const std::string title = Lower(s.title);
	if (s.value == VoteValue::Count && !value)
		return "no " + title;
	return title + " " + Core_Vote_SettingText(s, value);
}

std::string Core_Vote_Describe(const VoteProposal &p)
{
	const bool ready = p.kind == VoteKind::ReadyAll || p.kind == VoteKind::ReadyTeam;
	if ((Core_Vote_Setting(p.kind, false) || ready || p.kind == VoteKind::Shuffle) && !p.label.empty())
		return p.label; // "time limit 20 minutes", "ready red", "shuffle teams"
	const std::string &what = p.label.empty() ? p.arg : p.label;
	return what.empty() ? VoteKindName(p.kind) : std::string(VoteKindName(p.kind)) + " " + what;
}

std::vector<std::string> Core_Vote_Words(std::string_view text)
{
	std::vector<std::string> out;
	size_t i = 0;
	while (i < text.size())
	{
		while (i < text.size() && std::isspace((unsigned char)text[i]))
			++i;
		const size_t start = i;
		while (i < text.size() && !std::isspace((unsigned char)text[i]))
			++i;
		const std::string word = Lower(text.substr(start, i - start));
		const bool safe =
		    !word.empty() && word.size() <= 32 &&
		    std::all_of(word.begin(), word.end(), [](char c)
		                { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
		if (safe && !Contains(out, word))
			out.push_back(word);
	}
	return out;
}

std::vector<std::string> Core_Vote_MapList(std::string_view g_map_list, bool ctf)
{
	std::vector<std::string> maps = Core_Maps_Words(g_map_list);
	if (!maps.empty())
		return maps;
	if (ctf)
		return {"q2ctf1", "q2ctf2", "q2ctf3", "q2ctf4", "q2ctf5"};
	return {"q2dm1", "q2dm2", "q2dm3", "q2dm4", "q2dm5", "q2dm6", "q2dm7", "q2dm8"};
}

std::string Core_Vote_MapFor(const std::vector<std::string> &list, std::string_view current)
{
	const std::string cur = Lower(current);
	if (list.empty() || Contains(list, cur))
		return cur;
	return list.front();
}

std::string Core_Vote_ErrorText(VoteError e, const VoteConfig &config, const VoteProposal &p,
                                const VoteChoices &choices)
{
	switch (e)
	{
	case VoteError::None:
		return {};
	case VoteError::Disabled:
		return "Voting is off on this server.";
	case VoteError::NotAllowed:
	{
		std::vector<std::string> kinds;
		for (VoteKind k : config.allowed)
			kinds.push_back(VoteKindName(k));
		return std::string("Votes on ") + VoteKindName(p.kind) +
		       " are off. Allowed: " + (kinds.empty() ? "none" : Join(kinds)) + ".";
	}
	case VoteError::MatchLive:
		return "Votes can only be called before the match starts.";
	case VoteError::InProgress:
		return "A vote is already in progress.";
	case VoteError::NotVoter:
		if (p.kind == VoteKind::ReadyTeam && !p.arg.empty())
			return "Only " + Upper(p.arg) + " votes on readying " + Upper(p.arg) + ".";
		return "Only players in the match can vote.";
	case VoteError::LimitReached:
		return "You have called your " + std::to_string(config.call_limit) + " votes for this map.";
	case VoteError::Cooldown:
		return "Your last vote failed; wait a little before calling another.";
	case VoteError::BadArg:
		switch (p.kind)
		{
		case VoteKind::Map:
			return "Maps: " + Join(choices.maps) + ".";
		case VoteKind::Gametype:
			return "Gametypes: " + Join(choices.gametypes) + ".";
		case VoteKind::Restart:
			return "Usage: callvote restart";
		case VoteKind::ReadyAll:
			return "Usage: callvote readyall";
		case VoteKind::ReadyTeam:
			return "Join a team first.";
		case VoteKind::Shuffle:
			return "Usage: callvote shuffle";
		case VoteKind::Timelimit:
		case VoteKind::Scorelimit:
		case VoteKind::Powerups:
		case VoteKind::FriendlyFire:
		{
			const VoteSetting s = *Core_Vote_Setting(p.kind, choices.now.ctf);
			const std::string usage = std::string("Usage: callvote ") + VoteKindName(p.kind) + " ";
			if (s.value == VoteValue::OnOff)
				return usage + "on|off";
			return usage + std::to_string(s.min) + "-" + std::to_string(s.max) +
			       (s.value == VoteValue::Minutes ? " (minutes)" : " (0: none)");
		}
		}
		return {};
	case VoteError::AlreadySet:
	{
		const std::optional<VoteSetting> s = Core_Vote_Setting(p.kind, choices.now.ctf);
		const std::optional<int> v = s ? Core_Vote_SettingValue(*s, p.arg) : std::nullopt;
		return v ? "Already set: " + Core_Vote_SettingLabel(*s, *v) + "." : "Already set.";
	}
	case VoteError::TeamsOnly:
		return p.kind == VoteKind::ReadyTeam ? "Readyteam is only for team gametypes."
		       : p.kind == VoteKind::Shuffle ? "Shuffle is only for team gametypes."
		                                     : "Friendly fire is only for team gametypes.";
	case VoteError::WarmupOnly:
		return "Teams can only be shuffled in warmup.";
	case VoteError::TooFew:
		return "A shuffle needs at least three players on teams.";
	case VoteError::NoReadyUp:
		return "Nobody is readying up now: ready votes are for the warmup.";
	case VoteError::NoLimit:
		return "There's no time limit, so the match needs a score limit.";
	case VoteError::NoVote:
		return "No vote in progress.";
	case VoteError::AlreadyVoted:
		return "You already voted that way.";
	}
	return {};
}

void Vote::Reset()
{
	state_ = State::Idle;
	answers_.clear();
	tally_ = {};
	calls_.clear();
	failed_at_ms_.clear();
}

VoteError Vote::Call(int caller, VoteProposal proposal, const VoteChoices &choices, bool live,
                     const std::vector<int> &voters, int64_t now_ms)
{
	proposal.arg = Lower(proposal.arg);
	const bool server = caller == kServer;
	if (state_ != State::Idle)
		return VoteError::InProgress;
	if (!server)
	{
		if (!config_.enabled)
			return VoteError::Disabled;
		if (!config_.allowed.count(proposal.kind))
			return VoteError::NotAllowed;
		if (live && !config_.midgame)
			return VoteError::MatchLive;
		if (!IsVoter(voters, caller))
			return VoteError::NotVoter;
		if (config_.call_limit > 0 && calls_[caller] >= config_.call_limit)
			return VoteError::LimitReached;
		if (auto it = failed_at_ms_.find(caller);
		    it != failed_at_ms_.end() && now_ms - it->second < config_.fail_cooldown_ms)
			return VoteError::Cooldown;
	}

	bool arg_ok = false;
	switch (proposal.kind)
	{
	case VoteKind::Map:
		arg_ok = Contains(choices.maps, proposal.arg);
		break;
	case VoteKind::Gametype:
		arg_ok = Contains(choices.gametypes, proposal.arg);
		break;
	case VoteKind::Restart:
		arg_ok = proposal.arg.empty();
		break;
	case VoteKind::ReadyAll:
	case VoteKind::ReadyTeam:
	{
		const bool team = proposal.kind == VoteKind::ReadyTeam;
		if (team && !choices.now.teams)
			return VoteError::TeamsOnly;
		if (!choices.now.ready_up)
			return VoteError::NoReadyUp;
		arg_ok = team ? proposal.arg == "red" || proposal.arg == "blue" : proposal.arg.empty();
		proposal.label = team ? "ready " + proposal.arg : "ready everyone";
		break;
	}
	case VoteKind::Shuffle:
		if (!choices.now.teams)
			return VoteError::TeamsOnly;
		if (!choices.now.warmup)
			return VoteError::WarmupOnly;
		if (choices.now.team_players < 3)
			return VoteError::TooFew;
		arg_ok = proposal.arg.empty();
		proposal.label = "shuffle teams";
		break;
	case VoteKind::Timelimit:
	case VoteKind::Scorelimit:
	case VoteKind::Powerups:
	case VoteKind::FriendlyFire:
	{
		const VoteSetting s = *Core_Vote_Setting(proposal.kind, choices.now.ctf);
		if (s.teams_only && !choices.now.teams)
			return VoteError::TeamsOnly;
		const std::optional<int> value = Core_Vote_SettingValue(s, proposal.arg);
		if (!value)
			return VoteError::BadArg;
		proposal.arg = Core_Vote_SettingArg(s, *value);
		proposal.label = Core_Vote_SettingLabel(s, *value);
		if (*value == Core_Vote_SettingNow(s, choices.now))
			return VoteError::AlreadySet;
		if (proposal.kind == VoteKind::Scorelimit && *value == 0 && choices.now.timelimit_min <= 0)
			return VoteError::NoLimit;
		arg_ok = true;
		break;
	}
	}
	if (!arg_ok)
		return VoteError::BadArg;

	state_ = State::Open;
	proposal_ = std::move(proposal);
	caller_ = caller;
	since_ms_ = now_ms;
	answers_.clear();
	if (!server)
	{
		answers_[caller] = true;
		++calls_[caller];
	}
	Recount(voters);
	return VoteError::None;
}

VoteError Vote::Cast(int id, bool yes, const std::vector<int> &voters)
{
	if (state_ != State::Open)
		return VoteError::NoVote;
	if (!IsVoter(voters, id))
		return VoteError::NotVoter;
	if (auto it = answers_.find(id); it != answers_.end() && it->second == yes)
		return VoteError::AlreadyVoted;
	answers_[id] = yes;
	Recount(voters);
	return VoteError::None;
}

void Vote::Recount(const std::vector<int> &voters)
{
	tally_ = {};
	tally_.voters = (int)voters.size();
	for (int id : voters)
	{
		auto it = answers_.find(id);
		if (it == answers_.end())
			continue;
		(it->second ? tally_.yes : tally_.no)++;
	}
}

void Vote::Fail(int64_t now_ms)
{
	if (caller_ != kServer)
		failed_at_ms_[caller_] = now_ms;
	state_ = State::Idle;
	answers_.clear();
}

VoteEvent Vote::Update(const std::vector<int> &voters, int64_t now_ms)
{
	if (state_ == State::Passed)
	{
		if (now_ms - since_ms_ < config_.pass_delay_ms)
			return VoteEvent::None;
		state_ = State::Idle;
		answers_.clear();
		return VoteEvent::Execute;
	}
	if (state_ != State::Open)
		return VoteEvent::None;

	Recount(voters);
	// A majority of the voters there now; nobody left: it fails.
	const int half = tally_.voters / 2;
	if (tally_.voters > 0 && tally_.yes > half)
	{
		state_ = State::Passed;
		since_ms_ = now_ms;
		return VoteEvent::Passed;
	}
	if (tally_.voters == 0 || tally_.no >= tally_.voters - half)
	{
		Fail(now_ms);
		return VoteEvent::Failed;
	}
	if (now_ms - since_ms_ >= config_.timeout_ms)
	{
		Fail(now_ms);
		return VoteEvent::TimedOut;
	}
	return VoteEvent::None;
}

std::optional<bool> Vote::Answer(int id) const
{
	if (state_ != State::Open)
		return std::nullopt;
	auto it = answers_.find(id);
	return it == answers_.end() ? std::nullopt : std::optional<bool>(it->second);
}

int Vote::SecondsLeft(int64_t now_ms) const
{
	if (state_ != State::Open)
		return 0;
	const int64_t left = config_.timeout_ms - (now_ms - since_ms_);
	return left <= 0 ? 0 : (int)((left + 999) / 1000);
}

std::string Vote::HudText(int64_t now_ms) const
{
	if (state_ == State::Passed)
		return "VOTE PASSED";
	if (state_ != State::Open)
		return {};
	// A gametype shows by its name alone; the line has room for about 40 characters.
	const std::string what = proposal_.kind == VoteKind::Gametype && !proposal_.label.empty()
	                             ? proposal_.label
	                             : Core_Vote_Describe(proposal_);
	return "VOTE: " + Upper(what) + " - YES " + std::to_string(tally_.yes) + "/" + std::to_string(tally_.voters) +
	       " - " + std::to_string(SecondsLeft(now_ms)) + "s";
}
