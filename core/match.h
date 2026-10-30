// The match flow state machine (docs/design/match-flow.md), as pure logic.
//
// The adapter (core/match_register.cpp) builds a MatchInput from the level and
// clients each frame, calls Update, and carries out the returned actions. With
// MatchConfig::enabled false the machine is LIVE from the start and never acts:
// stock rules run the level, exactly as stock.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class MatchState
{
	Warmup,
	Countdown,
	Live,
	Overtime,
	RoundOver,      // a round ended: the survivors play on, the dead stay dead
	RoundCountdown, // then everyone respawned, counting down to the next round (frozen, or no firing)
	Intermission,
};

const char *MatchStateName(MatchState state);

// The match is being played: live, overtime, or between rounds. Not warmup,
// the countdown or the intermission.
bool MatchStateInPlay(MatchState state);

struct MatchPlayer;

// What ends a round (match_rounds; docs/design/match-flow.md).
enum class RoundsMode
{
	Off,         // 0: no rounds
	Frag,        // frag (or 1): any score change ends the round (HoonyMode)
	Elimination, // elimination: a side with no lives left loses the round (Clan Arena)
};

// match_rounds' value: "0", "1", "frag", "elimination" (any case); anything
// else is Off.
RoundsMode Core_Match_ParseRounds(std::string_view text);

// The one playing client other than `self`, or -1 if there isn't exactly one
// (match_suicide_point gives that opponent a suicide's point).
int Core_SuicideOpponent(const std::vector<MatchPlayer> &players, int self);

struct MatchConfig
{
	bool enabled = false;                // match_flow: false = stock (live immediately)
	bool ready_required = true;          // match_ready
	int min_players = 2;                 // match_minplayers: playing clients, bots too
	bool allow_no_humans = false;        // match_allow_no_humans: bots may play a match alone
	int64_t warmup_time_ms = 0;          // match_warmup_time; 0 = wait for ready
	int64_t countdown_ms = 10000;        // match_countdown
	int overtime = 0;                    // match_overtime: 0 none, -1 sudden death, N minutes
	int64_t timelimit_ms = 0;            // timelimit, from the start of LIVE; 0 = none
	int fraglimit = 0;                   // 0 = none
	RoundsMode rounds = RoundsMode::Off; // match_rounds
	int64_t round_pause_ms = 2000;       // match_round_pause: after the round ends, before the respawn
	int64_t round_countdown_ms = 5000;   // match_round_countdown: at the spawns, before the next round
	bool round_freeze = true;            // match_round_freeze: frozen in the round countdown; false: no firing
	int round_lives = 1;                 // match_round_lives: elimination: lives per player per round
	int64_t round_time_ms = 0;           // match_round_time: elimination: a round's length; 0 = none
	bool teams = false;                  // TDM, CTF: the lead and ties go by MatchInput::team_scores
	bool fraglimit_on_teams = false;     // TDM: fraglimit applies to team totals (stock CheckEndTDMLevel)
	int64_t join_grace_ms = 0;           // a new level: no countdown before this (players reloading the map)
	int64_t join_wait_ms = 0;            // ... nor, until this, while a human is still loading it

	bool operator==(const MatchConfig &o) const
	{
		return enabled == o.enabled && ready_required == o.ready_required && min_players == o.min_players &&
		       allow_no_humans == o.allow_no_humans && warmup_time_ms == o.warmup_time_ms &&
		       countdown_ms == o.countdown_ms && overtime == o.overtime && timelimit_ms == o.timelimit_ms &&
		       fraglimit == o.fraglimit && rounds == o.rounds && round_pause_ms == o.round_pause_ms &&
		       round_countdown_ms == o.round_countdown_ms && round_freeze == o.round_freeze &&
		       round_lives == o.round_lives && round_time_ms == o.round_time_ms && teams == o.teams &&
		       fraglimit_on_teams == o.fraglimit_on_teams && join_grace_ms == o.join_grace_ms &&
		       join_wait_ms == o.join_wait_ms;
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
	std::string key;      // who it is across slots (a social ID): an elimination round's roster
	int health = 0;       // health + armor: an elimination round's time-out tie-break
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
	Announce,     // text to every player (centre print)
	Klaxon,       // the stock klaxon, heard everywhere: overtime and sudden death begin
	Sound,        // text = a stock sound, heard everywhere: the spoken countdown, FIGHT, round won, match end
	Countdown,    // value = whole seconds left, sent once per second
	ResetArena,   // value 0: going LIVE - reset items, zero scores, respawn players
	              // value 1: between rounds - reset items and projectiles only
	RoundHold,    // the round is over: dead players stay dead (no respawn) until RoundRespawn
	RoundRespawn, // every playing client back in at a spawn, frozen until RoundStart
	RoundStart,   // the round begins: everyone released
	RoundWon,     // elimination: value = the side that won the round (team 1/2, or a client slot); -1 a draw
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
	// Whether playing clients are held in place (the round countdown with
	// match_round_freeze): they can look around but not move or fire.
	bool Frozen() const
	{
		return config_.enabled && state_ == MatchState::RoundCountdown && config_.round_freeze;
	}
	// Whether playing clients may move but not fire: the match countdown, and
	// the round countdown without match_round_freeze.
	bool HoldFire() const
	{
		return config_.enabled &&
		       (state_ == MatchState::Countdown || (state_ == MatchState::RoundCountdown && !config_.round_freeze));
	}
	// The round being played (with rounds): 1 from LIVE, +1 each new round.
	int Round() const
	{
		return round_;
	}
	// The HUD's match status and clock (docs/design/hud.md). The status, top
	// middle: OpenTDM's names (WAITING FOR PLAYERS / TEAMS, WARMUP, COUNTDOWN,
	// OVERTIME, SUDDEN DEATH, ROUND OVER, MATCH END), none in normal live
	// play. The clock, bottom middle, always: the match's length before it
	// starts, then the time left (or up from LIVE with no timelimit), 0:00 in
	// sudden death. Both empty with the match flow off.
	MatchHudText HudText(const MatchInput &in) const;
	// The HUD's big countdown digits: whole seconds left in the match or round
	// countdown, else 0 (hidden).
	int CountdownSeconds(int64_t now_ms) const;

	// A vote changed the time or frag limit (decision 0015). Before LIVE the
	// match just starts with them. In regulation time (LIVE, or between its
	// rounds) the end moves, still counted from LIVE: a limit already passed
	// ends the match at the next update, as stock's CheckDMRules would.
	// Overtime keeps its own end.
	void ChangeLimits(int64_t timelimit_ms, int fraglimit, int64_t now_ms);

	// The clock's end: the round's with an elimination round timer, else the
	// match's (the HUD and the scoreboard alike).
	int64_t ClockEndMs() const;

	// Elimination rounds (docs/design/clan-arena.md). A round's roster - who
	// plays it (slot, key, team) and their lives - is taken as it starts. A
	// side is a team, or without teams a player.
	bool Elimination() const
	{
		return config_.enabled && config_.rounds == RoundsMode::Elimination;
	}
	// A playing client died in an elimination round: a life gone. True if it
	// was their last (or they weren't in the round): they're out until the next.
	bool Died(int id);
	// Whether a client spawning now may play: anyone, except during an
	// elimination round, where only the roster's (same key, same team) with
	// lives left. Late joiners, team switchers and a new client in a leaver's
	// slot wait for the next round.
	bool InRound(int id, const std::string &key, int team) const;
	// A client left: their roster entry goes (a new client in the slot isn't them).
	void Forget(int id);
	// Out of the round under way (no lives left, or not on its roster), while
	// one is being played or just ended.
	bool Eliminated(int id) const;
	// Lives left among the roster's present players, per side (team 1, team 2;
	// or a slot without teams), for the HUD and "last one standing".
	std::map<int, int> SideLives(const MatchInput &in) const;
	// Rounds won by a side.
	int RoundWins(int side) const;

  private:
	bool CanStart(const MatchInput &in) const;
	bool ScoreChanged(const MatchInput &in) const;
	bool RoundBreak(const MatchInput &in, MatchState from, std::vector<MatchAction> &out);
	bool EliminationBreak(const MatchInput &in, MatchState from, std::vector<MatchAction> &out);
	void FillRoster(const MatchInput &in);
	void StartRound(const MatchInput &in, std::vector<MatchAction> &out);
	void ClearRounds();
	int SideOf(int id, int team) const;
	void SideScores(const MatchInput &in, int &top, int &second, int &count) const;
	void Baseline(const MatchInput &in);
	void GoLive(const MatchInput &in, std::vector<MatchAction> &out);
	void End(const char *why, std::vector<MatchAction> &out);
	void TimeWarning(int64_t now_ms, std::vector<MatchAction> &out);

	MatchConfig config_;
	MatchState state_ = MatchState::Live;
	int64_t state_since_ms_ = 0;
	int64_t live_since_ms_ = 0;
	int64_t time_end_ms_ = 0; // LIVE/OVERTIME deadline (0 = none)
	int last_countdown_s_ = -1;
	int last_warning_s_ = 0;                     // LIVE: the seconds left last spoken for (timelimit warnings)
	MatchState round_return_ = MatchState::Live; // where RoundCountdown goes back to
	int round_ = 0;
	std::vector<std::pair<int, int>> round_scores_; // (id, score) when the round began

	struct RoundEntry
	{
		std::string key;
		int team = 0;
		int lives = 0;
	};
	std::map<int, RoundEntry> roster_; // elimination: slot -> who plays this round
	std::map<int, int> round_wins_;    // elimination: side -> rounds won
	int64_t round_since_ms_ = 0;       // when the round under way started
	bool waiting_ = false;             // the round countdown waits for both sides
};
