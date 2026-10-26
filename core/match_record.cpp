// Pure part of the match record: JSON and the file name. No engine access.
#include "core/match_record.h"

#include <algorithm>
#include <json/json.h>

std::string Core_MatchRecord_ToJson(const MatchRecord &r)
{
	Json::Value root;
	root["schema"] = "q2game-match/0";
	root["mod"]["name"] = r.mod_name;
	root["mod"]["id"] = r.mod_id;
	root["mod"]["version"] = r.mod_version;
	root["mod"]["build"] = r.mod_build;
	root["gametype"] = r.gametype;
	root["ruleset"] = r.ruleset;
	root["map"] = r.map;
	root["ended_at"] = r.ended_at_utc;
	root["reason"] = r.reason;
	root["live_seconds"] = (Json::Int64)(r.live_ms / 1000);

	std::vector<MatchRecordPlayer> players = r.players;
	std::stable_sort(players.begin(), players.end(),
	                 [](const MatchRecordPlayer &a, const MatchRecordPlayer &b)
	                 {
		                 if (a.playing != b.playing)
			                 return a.playing; // players first, then spectators
		                 return a.score > b.score;
	                 });
	Json::Value list(Json::arrayValue);
	for (const MatchRecordPlayer &p : players)
	{
		Json::Value v;
		v["name"] = p.name;
		v["score"] = p.score;
		v["playing"] = p.playing;
		v["bot"] = p.bot;
		list.append(v);
	}
	root["players"] = list;

	int playing = 0;
	for (const MatchRecordPlayer &p : players)
		playing += p.playing;
	if (playing >= 2 && players[0].playing && players[0].score > players[1].score)
		root["winner"] = players[0].name;
	else if (playing == 1)
		root["winner"] = players[0].name;
	else
		root["winner"] = Json::nullValue;

	Json::StreamWriterBuilder w;
	w["indentation"] = "  ";
	return Json::writeString(w, root) + "\n";
}

std::string Core_MatchRecord_FileName(const std::string &ended_at_utc, const std::string &map)
{
	std::string stamp;
	for (char c : ended_at_utc)
	{
		if (c >= '0' && c <= '9')
			stamp += c;
		if (stamp.size() == 8 && c == 'T')
			stamp += '-';
	}
	std::string safe = map.empty() ? "unknown" : map;
	for (char &c : safe)
	{
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			c = '_';
	}
	return stamp + "-" + safe + ".json";
}
