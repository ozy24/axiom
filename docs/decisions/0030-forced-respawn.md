# 0030: The dead are respawned after 3 seconds, in every mode

**Decided:** 2026-11-16 (Chris: "it's critical for tdm and duel to stop
players from 'not being killed' if the timelimit is nearly run out").

Every built-in ruleset sets `g_dm_force_respawn 1` and
`g_dm_force_respawn_time 3`. Under a registered gametype a dead player may
respawn by pressing fire after 1 second, and is respawned anyway at 3.
This is how MuffMode ran (its defaults, which Bloody Blastard played
until it moved to Axiom on 2026-10-30).

**Why:** with stock's rules a player who is ahead can die and stay dead
until the time limit, and can't be fragged again: a Duel or a TDM match
won by hiding in death. Stock has no forced respawn by default.

**How it differs from stock:** stock reads `g_dm_force_respawn_time` as
the wait before any respawn, so `3` would also stop a player pressing
fire for 3 seconds. Under our gametypes it is the forced respawn's time
only, and pressing works after stock's 1 second (core/respawn.h).
Single player, coop and Rogue's stock gamerules modes keep stock's
reading. The compiled defaults stay stock's (0, 0); a mode switch resets
both, and the built-in ruleset sets them.

**An exception to "built-ins never change gameplay"** (they set match
rules and limits): this is a match rule, like the ready-up and the team
lock. An admin's ruleset turns it off with `set g_dm_force_respawn 0`.

**Tests:** `tests/test_respawn.cpp` covers the times. No game test yet:
Q2RED's simulated clients press fire every second or so by design
(`tools/sim-client/sim_client.cpp`, so respawn and intermission work), so
none can play the player who never presses. A `still` option there would
let a harness tier do it. A forced respawn prints `Respawn: <name>
respawned by the server` on the console.

**Reopen if:** players find 3 seconds too short to see who killed them
(the time is one setting per mode).
