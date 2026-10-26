# Status

_Updated at the end of every session. Last update: 2026-10-26._

## Current phase: 3a done (M3a); 3b next

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

Next: Phase 3b - TDM and CTF on the API, the team service (out of
`g_ctf.cpp`), CTF's match/election code replaced by the match flow.

## Known issues
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
