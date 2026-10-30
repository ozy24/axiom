# Clan Arena

Status: built (decision 0021). Change this page in the same PR as the code.

Clan Arena is team play in elimination rounds: everyone spawns with every
weapon and plenty of health and armor, there is nothing to pick up, and a
team with nobody left loses the round. It is its own gametype id, `ca`, so it
gets its own row in the vote menu and the Q2REX server browser's "Clan Arena"
label, but it has no code of its own: `gametypes/ca` is data, stock teamplay
runs underneath, and every rule is a match-flow setting the built-in ruleset
`ca` turns on. `g_gametype ca` without that ruleset plays TDM's rules.

## The built-in ruleset `ca`

| Setting | `ca` | Default | |
|---|---|---|---|
| `match_rounds` | `elimination` | `0` | `0`, `frag` (or `1`: HoonyMode's), `elimination` |
| `match_round_lives` | 1 | 1 | lives per player per round |
| `match_round_freeze` | 0 | 1 | round countdown: 1 frozen, 0 move but no fire |
| `match_round_countdown` | 5 | 5 | seconds before each round |
| `match_round_pause` | 2 | 2 | seconds after a round, before the respawn |
| `match_round_time` | 120 | 0 | a round's length in seconds; 0 = none |
| `fraglimit` | 8 | 0 | in elimination rounds: rounds to win ("first to 8") |
| `timelimit` | 0 | | checked only as a round ends |
| `match_overtime` | -1 | 0 | a tie at the timelimit: sudden death = the next decisive round |
| `match_arena` | 1 | 0 | the arena loadout, no items on the map, no drops |
| `match_arena_health` / `_armor` | 150 / 150 | 150 / 150 | health 1-999, body armor 0-200 |
| `match_damage_score` | 1 | 0 | +1 per 100 damage dealt to enemies |
| `g_dm_no_self_damage` | 1 | 0 | your own hits cost nothing; knockback stays |
| `g_dm_no_fall_damage` | 1 | 0 | stock's |
| `g_friendly_fire` | 0 | 0 | stock's |
| `match_lock` | 1 | 0 | teams lock as the match goes live |

## Rounds

- **The roster.** As a round starts, everyone playing is on its roster with
  `match_round_lives` lives. A death costs one; with lives left you respawn as
  normal, at 0 you're out of the round.
- **The end.** A team with no lives left loses the round. Both teams out in
  the same frame is a draw. When `match_round_time` runs out, the team with
  more lives left wins, then the one with more health and armor among the
  living, else a draw. A draw scores nothing.
- **The match.** The team score is rounds won. The winning round ends the
  match at once. The timelimit is checked only as a round ends, so the round
  under way is always played out; a tie then goes to sudden death, where the
  next round that isn't a draw decides it.
- **Empty team.** The next round doesn't start while a team has nobody on it
  (WAITING FOR TEAMS, the countdown starts again when it's back). Every human
  gone sends the match back to warmup, as in every mode.
- **The countdown.** `match_round_freeze 0` (CA): players move but can't fire
  or pick anything up; the match countdown is round 1's. `1` (frag rounds,
  and Quake Champions' Duel): frozen at the spawns; with elimination, round 1
  gets its own frozen countdown after the match countdown. Each round opens
  with "ROUND n: FIGHT!".

## Out of the round

- After about 2 seconds of the death view, an eliminated player follows a
  living teammate (+use flips to their eyes, fire moves on to the next). They
  never get a free camera: with no teammate to follow they're held where they
  are. They stay on their team (`sv match`: "(eliminated, following X)").
- **Late joiners, team switchers, a returning player** and a new client in a
  disconnected player's slot aren't on the roster: they watch until the next
  round ("A round is being played: you're in from the next.").
- **A disconnect** takes the player off the roster; they never count as alive.
  The team lock keeps their place for the next round.
- Everyone comes back for the next round, and when the deciding round ends
  the match, for the intermission.

## The arena

`match_arena 1`: every spawn has the blaster, shotgun, super shotgun,
machinegun, chaingun, grenade launcher, rocket launcher, hyperblaster and
railgun, each with ammo at the player's stock maximum, the rocket launcher in
hand, `match_arena_health` health and `match_arena_armor` body armor. No
item spawns on the map but keys and CTF flags; nothing drops on death.

## What players see

- The HUD's status line: `ROUND 3 - RED 3 v 2 BLUE` (lives left; with one
  life, players alive), `SUDDEN DEATH - ...` in overtime. The clock shows the
  round's time left when there's a round timer.
- Centre: "ROUND n: FIGHT!", "RED wins the round!" / "Round draw!", "You are
  the last one standing!" to a lone survivor.
- The team board: an `X` by each eliminated player (where ready-up's `R` goes),
  and "Round Limit: 8" for the limit. Match Info: "Rounds: first to 8",
  "Elimination: 1 life, 2:00".
- `callvote scorelimit` is a Round Limit (1-20).

## Compared with MMREX (and MuffMode)

MMREX's CA (`match/match.cpp`) has the same core rules. Fixed here:
- a mutual wipe stalls the round until the round timer (forever without one);
- a disconnected player's reservation counts as alive but can't be damaged;
- damage dealt between rounds, and each round's remainder under 100, is lost;
- "Round 3 of 8" counts rounds played against a limit of rounds won;
- 50 slugs against a cap of 25;
- the timelimit is only checked between rounds, with no warning;
- an empty team loses every round as its countdown ends.

## Quake Champions' Duel

The same rules with `match_round_lives 3`, `match_round_freeze 1`, a short
round countdown and `fraglimit 2` on Duel give QC's best-of-three: three
lives a round, frozen 3-2-1, the round ends when a player is out of lives.
Not a built-in yet; an admin can run it as `rulesets/duel_qc.cfg`.

## Implementation

- Pure: `core/match.{h,cpp}` (the roster, `EliminationBreak`, the HUD lines;
  `tests/test_match.cpp`), `core/arena.{h,cpp}` (the loadout, `DamageScore`;
  `tests/test_arena.cpp`), `core/scoreboard.cpp` (`Core_Scoreboard_TeamMarks`).
- Adapters: `core/match_register.cpp` (deaths, watching, `HoldOut`, the team
  score), `core/arena_register.cpp`.
- Marked hooks: `player_die` (a life), `PutClientInServer` (hold out, the
  loadout), `ClientThink` (fire for watchers, no free flight), `g_chase.cpp`
  (teammates only), `SpawnItem` (no items), `T_Damage` (no self damage, the
  damage score), `CTFScoreboardMessage` (X, Round Limit), `ClientDisconnect`.
- Harness: tier 7 (an elimination match with bots), tier 21 (scripted
  players), tier 4 (boots `ca`).
