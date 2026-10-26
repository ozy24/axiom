# Match flow

Status: **implemented (Phase 2b, part 1)**: states, cvars, arena reset steps 1-3,
harness tier 5. Not yet: rounds (with Duel, 3a), door/plat reset (step 4),
rulesets as `.cfg` (2b part 2). One state machine shared by every gametype.
Change this page in the same PR as the code.

## Principle: off means stock

With default settings the match flow is **off**: the level is live from the
first frame and stock `CheckDMRules` ends it on `timelimit`/`fraglimit`,
exactly as stock. The comparison tests (tiers 1-3) run with defaults, so they
keep proving stock behaviour. Everything below happens only when a ruleset
turns it on.

## States

```
            match_warmup 1                    all ready (or warmup timer)
 (map load) ──────────────► WARMUP ─────────────────────────────► COUNTDOWN
     │                       ▲   players play freely, no score       │ match_countdown s
     │ match_warmup 0        │   counts; `ready` toggles              │ (cancelled if a
     │                       └────── player un-readies / leaves ◄─────┘  needed player leaves)
     ▼                                                                ▼
   LIVE ◄─────────────────────────────────────────────── arena reset, scores zeroed
     │  timelimit / fraglimit / gametype condition
     ├── tied and match_overtime ≠ 0 ──► OVERTIME (sudden death or +N min) ──┐
     ▼                                                                      ▼
 INTERMISSION (stock intermission and map change)  ◄────────────────────────┘
```

- **WARMUP:** players can move, shoot and pick up; nothing is scored. Each
  player toggles `ready` (console command). The countdown starts when every
  required player is ready, and there are at least `match_minplayers`. If
  `match_warmup_time` > 0 it also starts when that runs out.
- **COUNTDOWN:** `match_countdown` seconds, announced by centre-print each
  second. If a required player un-readies or leaves, back to WARMUP.
- **LIVE:** on entry, the **arena reset** (below) runs and scores are zeroed.
  The end conditions are stock's (`timelimit`, `fraglimit`) plus the
  gametype's (`CheckDMRules` hook), so a gametype that ends on something else
  (rounds, captures) uses its hook.
- **OVERTIME:** when time runs out with the lead tied. `match_overtime 0`: no
  overtime (stock: ends tied). `-1`: sudden death (the next score wins).
  `N` > 0: N more minutes, repeated while tied.
- **INTERMISSION:** stock's.

Rounds (HoonyMode, Clan Arena) sit inside LIVE as a sub-state and arrive with
their first user, Duel's HoonyMode ruleset in Phase 3a.

## Arena reset

Puts the level back to its starting state without reloading the map, so a
match starts fair after warmup:

1. Remove dropped items, projectiles, gibs and other transient entities.
2. Put every item back as at map load (picked-up items respawn now; items with
   a respawn delay start as spawned).
3. Respawn every playing client at a fresh spawn point with the starting
   loadout; spectators stay spectators.
4. Reset doors, plats and buttons to their spawn positions where id's code
   allows it (best effort: these hold their own state). **Not done yet.**

Implemented in `core/match_register.cpp` (`ResetArena`). Bots count as always
ready; a client counts as playing only once it has begun (`pers.spawned`) and
isn't a spectator. Every transition prints a `Match:` console line.

## Cvars (neutral names, decision 0004)

| Cvar | Default | Meaning |
|---|---|---|
| `match_warmup` | `0` | 1: start in WARMUP (0 = stock: live immediately) |
| `match_ready` | `1` | in warmup, require every player to `ready` |
| `match_minplayers` | `2` | players needed before the countdown can start |
| `match_warmup_time` | `0` | seconds of warmup before starting anyway (0 = wait for ready) |
| `match_countdown` | `5` | seconds |
| `match_overtime` | `0` | 0 none, -1 sudden death, N minutes |

Rulesets (`.cfg`, decision 0003) set these; built-in defaults are stock.

## Implementation split

- `core/match.h/.cpp`: the state machine as **pure logic** over a small
  input snapshot (time, players with ready/playing/score, limits, cvars) that
  returns the new state and a list of actions (announce, reset arena, end
  match). Exhaustively unit-tested; no engine.
- `core/match_register.cpp`: the adapter. Builds the snapshot from `level` and
  clients each frame, carries out the actions, registers `ready`/`notready`.
  Called from marked one-line hooks in `G_RunFrame` and `ClientCommand`.
- CTF's own competition mode (`competition`, `ctfgame.match`) stays as stock
  until the CTF port in Phase 3b, where this replaces it.
