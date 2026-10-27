// Pure part of voting (core/vote.h). No engine access.
#include "core/vote.h"

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
	}
	return "?";
}

std::optional<VoteKind> Core_Vote_ParseKind(std::string_view name)
{
	const std::string n = Lower(name);
	for (VoteKind k : {VoteKind::Map, VoteKind::Gametype, VoteKind::Restart})
		if (n == VoteKindName(k))
			return k;
	if (n == "ruleset")
		return VoteKind::Gametype;
	return std::nullopt;
}

std::string Core_Vote_Describe(const VoteProposal &p)
{
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
	std::vector<std::string> maps = Core_Vote_Words(g_map_list);
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
		}
		return {};
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

std::string Vote::HudText() const
{
	if (state_ == State::Passed)
		return "VOTE PASSED";
	if (state_ != State::Open)
		return {};
	// A gametype shows by its name alone; the line has room for about 40 characters.
	const std::string what = proposal_.kind == VoteKind::Gametype && !proposal_.label.empty()
	                             ? proposal_.label
	                             : Core_Vote_Describe(proposal_);
	return "VOTE: " + Upper(what) + " (" + std::to_string(tally_.yes) + "/" + std::to_string(tally_.voters) + ")";
}
