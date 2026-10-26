// A finished match as a JSON record (the minimal Phase 3a version; full stats
// and a versioned schema for the ladder are Phase 5).
//
// Written by the match adapter at intermission when g_matchlog_dir is set:
// <dir>/<UTC yyyymmdd-hhmmss>-<map>.json.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MatchRecordPlayer
{
	std::string name;
	int score = 0;
	bool playing = true; // false: spectator or queued
	bool bot = false;
	std::string team; // team modes: "red" or "blue"; empty otherwise
};

struct MatchRecordTeam
{
	std::string name; // "red", "blue"
	int score = 0;    // TDM: frags; CTF: captures
};

struct MatchRecord
{
	std::string mod_name, mod_id, mod_version, mod_build;
	std::string gametype; // registry id, or "stock gamerules N"
	std::string ruleset;  // g_ruleset
	std::string map;
	std::string ended_at_utc; // ISO 8601
	std::string reason;       // timelimit, fraglimit, sudden death, gametype, rules...
	int64_t live_ms = 0;      // time from LIVE to the end
	std::vector<MatchRecordPlayer> players;
	std::vector<MatchRecordTeam> teams; // empty outside team modes; then the winner is a team
};

// The record as JSON: players sorted by score (highest first), and "winner"
// set when one playing client has the highest score alone.
std::string Core_MatchRecord_ToJson(const MatchRecord &record);

// "<yyyymmdd-hhmmss>-<map>.json", with anything outside [A-Za-z0-9_-] in the
// map name replaced, so a map can't steer the path.
std::string Core_MatchRecord_FileName(const std::string &ended_at_utc, const std::string &map);
