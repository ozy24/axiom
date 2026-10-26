# Simulated clients: game tests with "human" players

**Status:** S1 done (Q2RED #106, 2026-10-26); S2 and S3 next.

## Why

Bots and humans take different paths through the game module. Bots are
server-side clients (`SVF_BOT`, no network, no lobby). Humans connect over the
network, carry a lobby name token (`##P<n>`) and a social ID, get `stufftext`,
and reconnect as a new connection at every map change. The first playtest
(`docs/checklists/results/2026-10-26-playtest.md`) found three bugs, and two of
them lived only on the human path: the `##P2` name in `queue`, and a queue that
lost the rotation when clients re-entered after the map change. Bot-only harness
tiers cannot see that class of bug.

The goal is a test client that:

1. **joins the way a human does**: LAN lobby admission, `connect` with a
   social ID and userinfo, `new`/precache/`begin`, and reconnects at map change;
2. **plays well enough to matter**: it frags, dies, and sometimes kills
   itself, so scoring, rounds, captures and the match flow get exercised;
3. **does what a player does**: types `ready`, `notready`, `queue`, `team red`,
   `kill`, disconnects and rejoins, all on a script;
4. **shows what a player was shown**: prints, centre prints, the scoreboard
   layout, so a test can assert on them. This is recorded on the server by the
   **game tap** (Q2RED A63, `sv_game_tap <file>`), at the point the game module
   hands each message to the engine. Decoding it from the wire was tried first;
   the LAN transport batches datagrams and compresses parts of them, so that
   would have meant re-implementing the client's parser.

The manual playtest stays for what only a person can judge: feel, pacing, HUD
appearance, readability. It should get much shorter.

## What exists

- **Q2RED's synthetic client** (`docs/reverse-engineering/evidence-catalogs/platform/lan-load-probe/lan_client_probe.cpp`
  in Q2RED). It covers point 1 completely:
  - LAN admission, the Quake handshake, real `clc_move` packets built by Q2RED's
    own client packet writer, and the map-change reconnect. It ran 14 clients
    through 23 map changes in the two-hour Milestone 4 soak.
  - It already decodes `svc_locprint`.
  - It's a documented build recipe, not a CMake target, and its input is
    scripted wandering, not play.
- **Q2RED's bot host** (`src/server/sv_bot_host.c`, ledger A54). Once per
  frame, just before the game runs, the brain fills a usercmd for each bot
  client, and the host passes it to `SV_ClientThink`, the same call a human's
  packet reaches.
- **ozbot** (`q2red/oz_host.c`) plays deathmatch, TDM and CTF (`Ctf_Frame`) from
  that host contract, but only for clients it added itself.

## Design: a puppet behind a real connection

Point 2 is the hard part if the client must play from the network side. It
would have to decode entity snapshots and navigate the map from its own view,
which means writing a second bot. The plan doesn't do that. Instead **ozbot
plays the simulated client from inside the server**, while everything else
about the client stays human:

```
sim client (network)                  Q2RED                          game module
  lobby admission ─────────────────►  normal human client  ──────►  ClientConnect(social_id, isBot=false)
  connect, userinfo, begin            (not SVF_BOT)                  ClientBegin, userinfo, stufftext
  scripted stringcmds  ─────────────► ClientCommand  ─────────────►  ready / queue / team / kill
  clc_move (ignored for movement)     ozbot think(entnum) ─────────► ClientThink   ← the puppet's play
  ◄── prints, centre prints, layouts  ◄──────────────────────────── what a player sees
```

- **The game module can't tell it from a human.** It has no `SVF_BOT`, a real
  social ID, and a `##P` name, and it reconnects at map change.
- **Its movement comes from ozbot.** The server feeds ozbot's usercmd to
  `ClientThink` and discards the movement in the client's own `clc_move`. The
  client keeps sending packets (the netchan needs them) but doesn't steer.
- **Off unless asked for twice.** The server needs `sv_sim_puppets 1` (default
  0) **and** the client asks with a userinfo key (`\sim_puppet\1`). A production
  server never has the cvar on, so no real player can be puppeted.

## Phases

### S1: Q2RED: a scriptable sim client and the game tap (done, Q2RED #106)

- Promote the probe to a CMake target (`q2red_sim_client`, Linux only),
  installed by the jumpbox's `install-engines.sh` beside the server.
- **Script input** (a small JSON file per client):
  - name, social ID and extra userinfo;
  - a timeline of actions (`at 12.0: cmd "ready"`, `at 40.0: disconnect`,
    `at 45.0: rejoin`);
  - a total run time.
- **Output:** the sim client writes its own lifecycle (join, begin, each
  command, map changes, leaving) as JSON lines. The game tap
  (`sv_game_tap <file>`, off by default) writes what the game sent each
  slot: locprint (base and args, `##P` tokens as sent), print, centre print,
  layout, stufftext, configstring, connect, begin, disconnect.
- **Checked:** a Duel with one bot and two sim clients (ready, `queue`,
  disconnect and rejoin across a map change): 437 tap records, social IDs
  through, `##P` tokens as whole arguments, the countdown to each slot, and
  `queue` lines only to the asker.

### S2: Q2RED + ozbot: puppets (one Q2RED PR, one ozbot-re PR)

- **Q2RED:**
  - `sv_sim_puppets` cvar, default 0;
  - for a network client whose userinfo asks for it, call the brain's new
    `adopt(entnum)` at `ClientBegin`, including after a map change;
  - in `SV_BotHost_Frame`, think for adopted clients too;
  - in the packet path, keep their usercmd's timing but not its movement.
  - Bot brain API version +1, with `adopt`/`release` optional.
  - Ledger row (DIVERGENCE), off by default.
- **ozbot:** `adopt(entnum)` creates its bot state for an existing client
  without adding one. Adopted clients count as humans for `bot_minplayers` fill.
- **Test:** two puppets on q2dm1 for 60 s produce frags, and the game sees them
  as non-bots (`sv match` names, no `SVF_BOT`).

### S3: Axiom: scenario tiers (Axiom PRs)

- **Runner** (`scripts/harness/scenarios.py`). It reads the game tap for what
  each player was shown, and the sim client log for what they did:
  - starts a Q2RED rig with the LAN lobby, `sv_sim_puppets 1`, and optional
    ozbot bots for numbers;
  - starts up to seven sim clients (the LAN lobby's cap is eight, one held by
    the host);
  - gathers the tap, every client's event log and the server console, then
    runs the scenario's checks.
- **Scenarios** (`scripts/harness/scenarios/<name>.json` plus checks in
  Python), one per playtest section, first:
  - **Match flow:** `ready` is broadcast with the real name; the countdown
    reads 10…1 and "FIGHT!"; `notready` cancels; scores stay 0 and the layout
    has no `time_limit` in warmup; the clock appears at LIVE with the full
    timelimit.
  - **Duel:** the third client queues; `queue` lines carry decoded names,
    never `##P`; the match runs to a 15 s timelimit; after the map change the
    loser waits and the queued human plays, including a client that comes back
    after a delay.
  - **HoonyMode:** a frag ends the round, both are held, then respawn; the
    deciding frag ends the match.
  - **TDM / CTF:**
    - `team red` / `team blue` joins;
    - warmup needs both teams;
    - FIGHT! resets scores;
    - CTF: flags at base at LIVE, and a capture ends the match at
      `capturelimit 1`, if ozbot captures within a few minutes on q2ctf1.
      Otherwise this is checked through the record, not the HUD.
  - **Suicides:** a scripted `kill` in warmup leaves the score at 0, and in
    LIVE costs one.
- **CI:** scenario tiers run in `game-tests.yml` on the jumpbox, Q2RED only. The
  sim client and puppet support come with the Q2RED and ozbot pins.

### S4: retire checklist rows

Each playtest row that a scenario covers gets marked "automated" in
`docs/checklists/playtest.md`. What's left is the human list.

## Limits

- **Q2RED only.** Q2REX is not ours to change, so the MSVC DLL keeps its
  bot-only tiers. The game module code under test is the same; only the engine
  differs.
- **No rendering.** A layout string can be checked (for example, no clock in
  warmup), but not how the stock client draws it.
- **Seven clients per server** (the lobby cap). Plenty for every core gametype.
- **Timing isn't exact.** Q2RED frames run on wall-clock time, so checks allow
  a margin (as tier 5 already does), and scenarios use short timelimits.

## Order and size

S1 → S2 → S3, each a few focused PRs. S3 can start against S1 alone, with
idle sim clients for the join, queue, name and command checks. It gains
frag-dependent checks when S2 lands.
