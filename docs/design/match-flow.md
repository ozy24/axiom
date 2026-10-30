# Match flow

Status: **implemented (Phase 2b, part 1)**: states, cvars, the arena reset,
harness tier 5, rulesets. One state
machine shared by every gametype. No rounds: they went with Clan Arena and
HoonyMode (decision 0022; tag `clan-arena-v1`).
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
            match_flow 1                    all ready (or warmup timer)
 (map load) ──────────────► WARMUP ─────────────────────────────► COUNTDOWN
     │                       ▲   players play freely, no score       │ match_countdown s
     │ match_flow 0        │   counts; `ready` toggles              │ (cancelled if a
     │                       └────── player un-readies / leaves ◄─────┘  needed player leaves)
     ▼                                                                ▼
   LIVE ◄─────────────────────────────────────────────── arena reset, scores zeroed
     │  timelimit / fraglimit / gametype condition
     ├── tied and match_overtime ≠ 0 ──► OVERTIME (sudden death or +N min) ──┐
     ▼                                                                      ▼
 INTERMISSION (stock intermission and map change)  ◄────────────────────────┘
```

- **WARMUP:** players can move, shoot and pick up; nothing is scored. With
  `match_warmup_weapons 1` (the default) everyone spawns with every weapon
  the map places, with full ammo and the best of them in hand, and drops
  nothing when they die, so the floor doesn't fill with weapons (Chris,
  2026-10-29). The log names the loadout once a level (`Warmup: loadout
  ...`). The match going live respawns everyone with stock's start. Each
  player toggles `ready` (console command). The countdown starts when every
  required player is ready, and there are at least `match_minplayers`
  playing, bots counted (as MuffMode's `minplayers`: one human and the
  engine's bot fill can start a match). A match also needs a human: bots
  alone only ever warm up, unless `match_allow_no_humans 1` (Chris,
  2026-10-29). If
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
  second. Players can move but neither fire nor pick anything up; the dead
  can still press fire to respawn. If a required player un-readies or
  leaves, back to WARMUP.
- **LIVE:** on entry, the **arena reset** (below) runs and scores are zeroed.
  The end conditions are stock's (`timelimit`, `fraglimit`) plus the
  gametype's (`CheckDMRules` hook), so a gametype that ends on something else
  (captures) uses its hook. While one human is playing the match
  goes on; when every human has left (LIVE or overtime),
  it goes back to WARMUP with the arena reset and scores zeroed, so the
  next to arrive don't find a match the bots played (not with
  `match_allow_no_humans 1`). Dropping below `match_minplayers` doesn't end
  a match (MuffMode's does).
- **Teams** (TDM, CTF): the match needs someone playing on each team to start
  (a client on no team isn't playing), and the lead, ties and sudden death go
  by **team score**: frags in TDM, captures in CTF. TDM's `fraglimit` applies
  to team totals and CTF's to players, both as stock; CTF ends at
  `capturelimit` through its `match_over` hook. Going LIVE also zeroes the
  team scores and sends the flags home.
- **OVERTIME:** when time runs out with the lead tied. `match_overtime 0`: no
  overtime (stock: ends tied). `-1`: sudden death (the next score wins).
  `N` > 0: N more minutes, repeated while tied (the built-in Duel, TDM and
  CTF: 2; FFA: sudden death; Chris, 2026-10-30). `match_overtime_max` M > 0:
  still tied M minutes into overtime (sudden death's, or at the end of a
  period), the match ends a draw, as MMREX's `g_dm_tie_max_time` (30 in
  every built-in, as MMREX's default; compiled 0). The HUD names it for 10
  seconds as it begins and as each new period starts, then only the clock
  shows (Chris, 2026-10-30).
- **INTERMISSION:** stock's.

## Arena reset

Puts the level back to its starting state without reloading the map, so a
match starts fair after warmup:

1. Put the movers back where they were as the level began: doors, plats,
   buttons, trains, rotating and water brushes, their triggers and timers,
   and the area portals the doors open (`core/movers_register.cpp`). Each
   keeps its whole state in its own entity, so a copy of its position,
   `moveinfo`, think and the rest is taken half a second into the level
   (after id's movers finish setting up in their first think: a door spawns
   its trigger, a train finds its first corner) and copied back. A mover
   destroyed since (a broken `func_explosive`, a used `trigger_once`) stays
   gone. First, so players respawn clear of them. `Movers: N reset` in the
   log; tier 5 checks q2dm1's two plats. MuffMode only unsticks idle trains;
   MMREX rebuilds the whole map from its entity string.
2. Remove dropped items, projectiles, gibs, a trigger's delayed firing and
   other transient entities.
3. Put every item back as at map load (picked-up items respawn now; items with
   a respawn delay start as spawned).
4. Respawn every playing client at a fresh spawn point with the starting
   loadout; spectators stay spectators.

Implemented in `core/match_register.cpp` (`ResetArena`). Bots count as always
ready; a client counts as playing only once it has begun (`pers.spawned`) and
isn't a spectator. Every transition prints a `Match:` console line.

## Cvars (neutral names, decision 0004)

| Cvar | Default | Meaning |
|---|---|---|
| `match_flow` | `0` | 1: start in WARMUP (0 = stock: live immediately) |
| `match_ready` | `1` | in warmup, require every player to `ready` |
| `match_minplayers` | `2` | players needed before the countdown can start, bots counted |
| `match_allow_no_humans` | `0` | 1: bots may start and play a match with no human (a bot server); 0: a match needs a human, and goes back to warmup when the last one leaves |
| `match_warmup_time` | `0` | seconds of warmup before starting anyway (0 = wait for ready) |
| `match_countdown` | `10` | seconds (10 since the 2026-10-26 playtest) |
| `match_overtime` | `0` | 0 none, -1 sudden death, N minutes |
| `match_overtime_max` | `0` | M > 0: still tied M minutes into overtime, the match is a draw; 0 never (built-ins: 30) |
| `match_warmup_weapons` | `1` | warmup: every weapon the map places, full ammo, no drops on death |
| `match_lock` | `0` | 1: both teams lock as the match goes live (team modes; docs/design/teams.md) |

Rulesets set these (decisions 0003 and 0013, `core/rulesets.h`, the admin's
view in docs/admin.md). The compiled defaults are stock; each gametype's
built-in ruleset (`ffa`, `duel`, `tdm`, `ctf`) turns the match flow on, and
`rulesets/<gametype>.cfg` changes it. `sv ruleset <name>` (or `set g_ruleset`
in server.cfg) switches: every gameplay cvar back to its default, the
gametype's settings and cfg, then a variant's (an admin's `ctf_instagib`, say). `g_ruleset`
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
