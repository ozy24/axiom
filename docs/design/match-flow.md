# Match flow

Status: **implemented (Phase 2b, part 1)**: states, cvars, arena reset steps 1-3,
harness tiers 5 and 7, rulesets, rounds. Not yet: door/plat reset (step 4),
elimination rounds. One state machine shared by every gametype.
Change this page in the same PR as the code.

## Principle: off means stock

With default settings the match flow is **off**: the level is live from the
first frame and stock `CheckDMRules` ends it on `timelimit`/`fraglimit`,
exactly as stock. The comparison tests (tiers 1-3) run with defaults, so they
keep proving stock behaviour. Everything below happens only when a ruleset
turns it on.

Off means stock *rules*. The HUD additions (the clock, on stock's
timelimit; the miniscore; the name line, `docs/design/hud.md`) still show
under a registered gametype: they change what players see, not how the
game plays.

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
  `match_warmup_time` > 0 it also starts when that runs out. It never starts
  in a level's first 10 seconds, nor while a human is still loading the map
  (up to 30 seconds in, so a stuck client can't hold it):
  after a map change the bots are back first and ready, and the countdown
  used to start, then be cancelled as the humans arrived unready (playtest,
  2026-10-27).
- **Free play until LIVE:** in WARMUP and COUNTDOWN nothing scores. Frags,
  suicides and captures are undone in the frame they happen, before any client
  sees them, and the scoreboard shows no clock. From LIVE the clock runs to the
  timelimit (then timed overtime); sudden death has no clock. `sv match`
  prints the state, every client's score and the team scores.
- **COUNTDOWN:** `match_countdown` seconds, announced by centre-print each
  second. If a required player un-readies or leaves, back to WARMUP.
- **LIVE:** on entry, the **arena reset** (below) runs and scores are zeroed.
  The end conditions are stock's (`timelimit`, `fraglimit`) plus the
  gametype's (`CheckDMRules` hook), so a gametype that ends on something else
  (rounds, captures) uses its hook.
- **Teams** (TDM, CTF): the match needs someone playing on each team to start
  (a client on no team isn't playing), and the lead, ties and sudden death go
  by **team score**: frags in TDM, captures in CTF. TDM's `fraglimit` applies
  to team totals and CTF's to players, both as stock; CTF ends at
  `capturelimit` through its `match_over` hook. Going LIVE also zeroes the
  team scores and sends the flags home.
- **OVERTIME:** when time runs out with the lead tied. `match_overtime 0`: no
  overtime (stock: ends tied). `-1`: sudden death (the next score wins).
  `N` > 0: N more minutes, repeated while tied.
- **INTERMISSION:** stock's.

**Rounds** (`match_rounds 1`, HoonyMode): while LIVE or OVERTIME, any change
in a playing client's score ends the round, unless it ends the match
(fraglimit, sudden death and forfeits are checked first). Then:

1. **ROUND OVER** for `match_round_pause` seconds (2): the winner plays on,
   the dead stay dead and can't respawn, and the kill message is on screen.
2. **ROUND COUNTDOWN** for `match_round_countdown` seconds (5): items back,
   every playing client respawned at a spawn point with a fresh loadout, and
   frozen - they can look around but not move or fire
   (`Core_Match_Frozen`, a marked early return in `ClientThink`).
3. FIGHT!: released, and the state returns to LIVE or OVERTIME.

Scores are kept and the timelimit keeps running; if it runs out in the pause,
everyone is respawned before the match decides. Clan Arena-style rounds (end
on elimination) would reuse the same states.

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
| `match_countdown` | `10` | seconds (10 since the 2026-10-26 playtest) |
| `match_overtime` | `0` | 0 none, -1 sudden death, N minutes |
| `match_rounds` | `0` | 1: every score change ends a round (HoonyMode) |
| `match_round_countdown` | `5` | seconds frozen at the spawns before a round (rounds on) |
| `match_suicide_point` | `0` | 1: a suicide or death to the world is a point to the only opponent, not -1 (HoonyMode; not in team modes or with more than one opponent) |
| `match_round_pause` | `2` | seconds after the round-ending frag before the respawn (rounds on) |

Rulesets set these (decisions 0003 and 0013, `core/rulesets.h`, the admin's
view in docs/admin.md). The compiled defaults are stock; each gametype's
built-in ruleset (`ffa`, `duel`, `tdm`, `ctf`) turns the match flow on, and
`rulesets/<gametype>.cfg` changes it. `sv ruleset <name>` (or `set g_ruleset`
in server.cfg) switches: every gameplay cvar back to its default, the
gametype's settings and cfg, then a variant's (`hoony`). `g_ruleset`
(serverinfo) names the mode; empty means server.cfg set everything itself.
The module can't see whether an exec found its file, so a missing one shows
only the engine's `couldn't exec` warning.

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
