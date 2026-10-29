# Voting

Players change the map or gametype, restart the match, or change one of a
few settings, by majority vote. Console commands are the interface; the Tab menu issues the
same commands. Pure logic: `core/vote.{h,cpp}` (tests: `tests/test_vote.cpp`);
engine side: `core/vote_register.cpp`; harness: tiers 14, 16, 19 and 20.

## Commands

| Command | Does |
|---|---|
| `callvote map <name>` | a map on the map list |
| `callvote gametype <name>` | a gametype (`ffa`, `duel`, `tdm`, `ctf`) or a variant (an admin's own listed in `g_rulesets`; HoonyMode, the built-in Duel variant, is off for now): switches to that ruleset as `sv ruleset` does (decision 0013), then reloads. The current settings don't carry over (HoonyMode's rounds don't follow a vote to TDM). `callvote ruleset` is the same command. |
| `callvote restart` | reloads the current map: a fresh warmup |
| `callvote timelimit <1-60>` | the time limit in minutes, at once (no restart): still counted from LIVE |
| `callvote scorelimit <n>` | the frag limit (0-100), or the capture limit in CTF (0-20); 0 = none. `fraglimit` and `capturelimit` are the same command |
| `callvote powerups on\|off` | stock's `g_no_items`; reloads the map, as items spawn when it loads |
| `callvote friendlyfire on\|off` | team modes only; at once |
| `callvote readyall` | in a ready-up warmup: everyone playing is ready |
| `callvote readyteam` | team modes, in a ready-up warmup: the caller's team is ready. Only that team votes on it (a majority of its humans); the HUD shows `VOTE: READY RED`. `sv callvote readyteam red\|blue` from the console |
| `callvote shuffle` | team modes, in warmup (or the countdown), with three or more players on teams: splits them into two new teams at random, sizes differing by at most one and never the same two groups back (decision 0020). Unlocks both teams; everyone on a team respawns. Refused once the match is live, and with the match flow off |
| `vote yes` / `vote no` | answer the open vote; an answer can be changed |
| `yes` / `no` | stock's commands: answer our vote while one is open, else stock's CTF elections as before |
| `warp <map>` | stock's map election: the same as `callvote map` |

A passed map, gametype, restart or powerups vote ends in `map <name>` (a
fresh load, so latched settings such as `g_gametype` apply), never through
intermission: a warmup vote must
not write a match record or rotate the Duel queue. A gametype
change keeps the current map if the new map list has it, else takes the
list's first map (FFA on q2dm1 voted to CTF goes to q2ctf1).

## Gametypes and their variants

Players vote on gametypes; rulesets are the settings behind them, not a
separate vote. Each built-in ruleset belongs to the gametype it selects:
each gametype has its own (`ffa`, `duel`, `tdm`, `ctf`), and variants
play on top of one: an admin's `rulesets/<gametype>_<name>.cfg` listed in `g_rulesets` ("ctf_instagib" is
"Capture The Flag: Instagib"). HoonyMode ("Duel: HoonyMode") was a
built-in variant and is off for now (2026-10-29); Clan Arena, next, is a
built-in TDM variant the same way. docs/admin.md has the admin's side.

## Rules

- **Who votes:** humans in the match (playing; in Duel, everyone in the
  queue). Spectators and bots don't count and can't call.
- The caller's vote is a yes. A vote **passes** on more than half the voters
  there now, recounted every frame (leavers drop out, joiners count),
  **fails** as soon as it can't pass, and **times out** after `g_vote_time`.
  A passed vote runs 3 seconds later.
- One vote at a time; none during intermission.
- Votes can be called **during a live match** too (`g_vote_midgame 1`, the
  default), so a match started with the wrong settings can be voted away.
  `g_vote_midgame 0` limits them to warmup (and any time with the match flow
  off).
- Setting votes (decision 0015) come from one table compiled in. A voted
  value lasts until the gametype changes; a vote for the value it has now
  is refused, and so is no score limit with no time limit. The limits and
  friendly fire apply in place; a time limit already passed ends the match.
- The other kinds end in a full reload of a map, so a vote mid-match restarts
  the match from warmup. Each kind needs it: a map change by definition;
  `g_gametype` is latched; a ruleset can change the gametype, item settings
  (spawned at load) and the match settings (read when the level starts);
  and a reload is the restart that leaves nothing of the old match behind.
  (MMREX also reloads for its powerups and techs votes, for the same
  item-spawn reason; its restart resets the match in place.)
- Each player calls at most `g_vote_limit` votes per map, and after one of
  theirs fails waits `g_vote_cooldown` seconds before calling another.

## Settings

| Cvar | Default | |
|---|---|---|
| `g_vote` | 1 | voting on |
| `g_vote_allow` | `map gametype restart timelimit scorelimit powerups friendlyfire readyall readyteam shuffle` | the kinds that can be called |
| `g_vote_midgame` | 1 | also during a live match; 0 = warmup only |
| `g_vote_time` | 30 | seconds to vote (minimum 5) |
| `g_vote_cooldown` | 30 | seconds after a caller's failed vote |
| `g_vote_limit` | 3 | calls per player per map; 0 = no limit |
| `g_map_list` | (empty) | the rotation (decision 0014), which a vote picks from; empty = q2dm1-8, or q2ctf1-5 under CTF |

List words are lowercased and must be `[a-z0-9_-]` (maps may also have
`/` between parts, as `q64/dm1`), up to 32 characters; anything else is
dropped, so a list can't inject a console command.

## What players see

- The HUD's single status line (docs/design/hud.md) shows the vote instead
  of the match status while it is open, `VOTE: MAP Q2DM3 - YES 1/3 - 27s`
  or `VOTE: DUEL: HOONYMODE - YES 1/3 - 27s` (yes votes / voters, seconds
  left), then `VOTE PASSED`.
- A broadcast when a vote is called, passes, fails or times out; the call
  plays `misc/pc_up.wav` to every player (MuffMode's vote sound).
- `sv callvote <kind> [choice]`: the admin puts a vote to the players. The
  server has no answer of its own; g_vote, g_vote_allow, the midgame rule,
  the call limit and the cooldown don't apply; the lists still do.
- The Tab menu: while a vote is open, a voter's main page leads with the
  proposal and **Vote Yes / Vote No** (the cursor starts on Vote Yes; their
  current answer is marked and skipped). When they can call one, **Call a
  Vote** opens Map / Gametype / Restart Match (and in a team game's warmup with three players, Shuffle Teams); the lists mark the current
  choice. Under Gametype, one with a single ruleset (TDM, CTF) is voted
  from the list; one with variants opens them ("Capture The Flag",
  "Instagib"). A list longer than the page (14 rows) ends in "More:
  callvote <kind>".

## Compared with MMREX

MMREX (`match/vote.cpp`, `ui/vote_menu.cpp`, ~3,000 lines) has 17 vote
kinds, a disable bitmask (`g_vote_flags`), two map cvars (`g_map_list`,
`g_map_pool`), and forces a ballot menu onto every voter. Axiom keeps its
rules (majority of eligible humans, caller votes yes, 30 s, 3 s delay,
per-map limit) and differs where noted above: seven kinds (four of them
settings from one table, where MMREX writes a handler pair per kind), an
allow-list, one map list, no forced menu, a cooldown, and pure tested
logic shared by menu and console.

Map rotation is decision 0014; there is no end-of-match map pick.
