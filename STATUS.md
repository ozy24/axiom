# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 3b done (M3b); Phase 4 next

## Phase 2b, part 1 (match flow) done
- `core/match.*` state machine (24 unit-test cases) and adapter: warmup,
  ready/notready, countdown, live (timelimit from LIVE), overtime (sudden
  death or N min), intermission. Off by default: `match_warmup 0` is stock,
  so tiers 1-3 keep proving stock behaviour.
- Arena reset at LIVE: projectiles and dropped items removed, map items
  respawned, scores zeroed, players respawned. Doors/plats not reset yet.
- Harness tier 5. Bots keep fighting after the reset (7 kills in the minute
  after LIVE vs 10 in a stock minute, checked by hand).

## Phase 2b, part 2 (rulesets) done
- `sv ruleset <name>` / `sv rulesets` (`core/rulesets.h`): built-ins `stock`
  and `match` compiled in; other names exec `rulesets/<name>.cfg`. Safe names
  only. Unit tests check every built-in sets only known cvars.

## Phase 3a, part 1 (Duel) done
- `gametypes/duel/`: a play queue (pure, unit-tested) — the front two play,
  everyone else spectates; newcomers spawn straight in as spectators via
  `DMGame.ClientBegin` (gate relaxed, `[mod]`); `queue` lists it; a duellist
  leaving is a forfeit; the loser rotates to the back, the winner stays on.
- New optional `GametypeDef` hooks, driven by the match flow's tick:
  `frame`, `match_over`, `match_ended`, `client_command`.
- Built-in ruleset `duel`; harness tier 6.
- Known: two ozbot bots on q2dm1 score nothing in a minute even in plain FFA,
  so the Duel test checks flow, queueing and rotation, not frags.
- Known (stock): with only bots, DM intermission never exits (stock waits for
  a player to press a button).

## Phase 3a, part 2 (HoonyMode) done
- Rounds in the match flow (`match_rounds`, `match_round_countdown`): a
  frag ends the round, players are held, the level resets (scores kept),
  everyone respawns together. 14 more unit-test cases; harness tier 7.
- Built-in ruleset `hoony`: Duel with rounds, first to 7, sudden death.
- Every built-in ruleset now sets every match cvar (a unit test checks), so
  switching rulesets never leaves one behind.

## Phase 3a, part 3 (match records) done
- `g_matchlog_dir`: at intermission, one JSON record per match
  (`<UTC>-<map>.json`, schema `q2game-match/0`): mod, gametype, ruleset,
  map, reason, live seconds, players (best first, spectators last), winner.
  Pure builder unit-tested (escaping, ties, file names can't be steered).
- Fixed: Duel's queued clients were flipping in and out every frame
  (`PutClientInServer` re-reads the `spectator` userinfo key). Tier 6 now
  catches flapping and checks the record.

## Phase 3a, part 4 (lobby metadata) done
- `GametypeDef::retail_mode`; each game load publishes Q2REX's lobby cvars
  (`q2rex_lobby_mode` = id, `_mode_name` = title, "(rounds)" with rounds on,
  `_retail_mode`, `_score_kind`). Retail browsers keep seeing deathmatch.
  Q2RED publishes them too since its A60 (pinned from 13daec66), so Q2REX
  browsers list an Axiom server on Q2RED as "Duel".
- A ruleset's latched g_gametype applies at the next `map`: the engine
  restarts the game module (checked on Q2RED).

## Phase 3a, part 5 (host file access) done
- Three Q2RED changes, merged: lobby discovery (A60, #103), 1024-byte
  paths on POSIX (A61, #104), file access for the game module through
  `GetExtension` (A62, #105). Pin bumped to 13daec66.
- `core/host_files*`: uses Q2RED's file access when offered (decision
  0011). A relative `g_matchlog_dir` lands in the game directory on Q2RED;
  elsewhere it's relative to the working directory, as before.
- The harness rig-path limit is 900 (was 56, Q2RED's old 128-byte paths).

**M3a reached** (Duel with a bot match on both engines, match JSON written).
Playtest is the open human gate for it.

## Phase 3b (TDM and CTF) done
- `tdm` and `ctf` registry gametypes on stock's team code (decision 0012:
  no team-service extraction). `Core_Gametype_PreInit` sets stock's
  `teamplay` / `ctf` from the gametype; `ctf 1` / `teamplay 1` with
  `g_gametype ffa` still selects them (stock configs keep working), so every
  built-in ruleset sets both. Rulesets `tdm` and `ctf`.
- The match flow is team-aware: start needs both teams staffed; the lead,
  ties and sudden death go by team score (TDM frags, CTF captures); TDM's
  fraglimit is on team totals; CTF ends at `capturelimit` (`match_over` now
  returns its reason). Going LIVE zeroes team scores and sends flags home.
  Match records list the teams; the winner is a team.
- Fixed: with the match flow off, `ready` / `notready` were swallowed
  ("There is no match warmup"), so stock CTF competition mode's ready-up
  couldn't work. Not reproducible in the harness (no human client); playtest
  row 18.
- Harness: tier 4 boots tdm and ctf (ctf on q2ctf1); new tier 8 plays a TDM
  and a CTF match on both engines.

## Playtest fixes (2026-10-26)
- Warmup and countdown are free play: scores held at 0 (frags, suicides,
  captures), and the scoreboard clock shows nothing until LIVE, then counts
  the timelimit from LIVE (was: from map load). Found in Chris's playtest.
  New `sv match` status command; harness tier 9 reproduces the scoring bug.
- Duel (rows 1-9, `docs/checklists/results/2026-10-26-playtest.md`):
  countdown now 10 s; `queue` showed lobby names as `##P<n>` (the KEX name
  token is decoded only as a whole argument; records and console now use the
  real name, `core/player_name.h`); the queue lost the rotation at a level
  change (every client re-enters one by one) - now kept for 30 s into a
  level, keyed by social ID so a human reconnecting into another slot keeps
  their place (as MMREX does), harness tier 10 (bots only: the human
  reconnect path is checked in the row 9 retest). HoonyMode, TDM, CTF rows still to test.

**M3b reached** (TDM and CTF with a harness bot match on both engines, once
CI is green). Playtest rows 14-18 are the open human gate.

Next: simulated clients for the game tests (`docs/design/simulated-clients.md`,
phases S1-S4: a scriptable Q2RED sim client, ozbot-driven puppets behind a
real connection, scenario tiers per gametype), then Phase 4 - competitive
essentials (spectating, timeouts, admin, scoreboards, simple voting).

## Simulated players (S1-S3 of `docs/design/simulated-clients.md`)
- Q2RED #106: `q2red_sim_client` (scripted players through the LAN lobby) and
  the game tap (`sv_game_tap`, A63: what the game sends each client). #107:
  puppets (A64): ozbot plays a sim client; the game still sees a human.
- ozbot-re #5 (adopt) merged as 80ddecde; pins moved to Q2RED fa193230
  and ozbot 80ddecde (installed on the jumpbox), so tier 11 runs in CI.
  Claude's merge policy covers Axiom and Q2RED, not ozbot-re.
- Harness tier 11 human-duel: playtest rows 1-9 through the human path.
  Against the pre-playtest build it fails on every bug Chris found.
- Tier 12 human-teams (TDM, CTF: team joins, no start with one team, the
  record's teams) and tier 13 human-rounds (HoonyMode rounds ended by
  scripted suicides). Scenario settings go in an exec'd cfg: too many
  `+set`s pass the engine's 50-argument limit, which also showed Q2RED
  segfaulting after that fatal error (fixed, Q2RED #108, A65).
- Not covered yet: a CTF capture (ozbot capturing on q2ctf1 within a short
  test is unproven) and leaving intermission by a button press (ozbot
  presses none; scenarios issue `gamemap`). Playtest rows 1-17 now have
  automated equivalents; row 18 and feel/HUD stay manual.

## HoonyMode round flow (2026-10-27, from the playtest)
- A frag ends the round: ROUND OVER for `match_round_pause` (2 s, the winner
  plays on, the dead stay dead), then everyone respawns frozen for
  `match_round_countdown` (now 5 s), then FIGHT!. Fixes the vanishing bot
  (the old hold overwrote svflags, dropping SVF_BOT).

## Known issues
- One intermittent Q2RED segfault in a CTF server with simulated players
  (tier 12, 2026-10-27, once under `--jobs 4`); not reproduced in 16 more
  runs. CI now keeps CRASHLOG.TXT (collect-logs.sh); look there if it recurs.
- Q2REX's built-in bots never enter a bot-only match (retail admits bots
  only after a human spawns), so tier 3 can't check frags on Q2REX.
- `jumpbox.lan` returns a bogus AAAA record (`::`); use `ssh -4`.
- The save-name baseline includes 49 names the compiler drops as unused, so
  saves can never contain them. Harmless; the check just errs on the safe side.

## Open human gates
- Single-player checklist for M2a, whenever convenient (any build from now on).
- **Playtest** (`docs/checklists/playtest.md`): match flow, Duel and HoonyMode in
  a retail client. Rig: `scripts/playtest/setup-local-rig.sh` -> `~/q2playtest`,
  join with `connect 127.0.0.1:5073`. Doesn't block further work.

## Local setup notes
- Format tools: `python3 -m venv ~/.local/share/q2game-tools &&
  ~/.local/share/q2game-tools/bin/pip install -r scripts/requirements-dev.txt`
  (`scripts/check_format.sh` finds it automatically).
