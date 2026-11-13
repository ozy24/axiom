# Changelog

Every release, newest first. Version numbers follow `mod.json`; until 1.0
the server settings and the downstream API (docs/downstream.md) may still
change, and every such change is listed here.

## Unreleased

### Settings
- **`g_no_blood`** (default 0): sparks instead of blood on hits, and no
  flesh gibs (a gibbed body's pieces and head are not drawn). Server-side,
  so stock clients need nothing (docs/admin.md, "No blood").

## 0.1.0 (2026-11-13): the first public release

A Quake II Rerelease game module for competitive and public servers, built
on id's GPL game source and a drop-in replacement for `baseq2/game_x64.dll`.
Single player, coop and the mission packs play exactly as stock; stock
clients need nothing installed.

### Gametypes
- **Free For All, Duel, Team Deathmatch and Capture The Flag**, chosen
  with `g_gametype` or a vote. TDM and CTF run on stock's team code; Duel
  adds a play queue (the loser goes to the back; leaving a live duel is a
  forfeit) and its own scoreboard. Rogue's Tag and DeathBall still run
  through stock's `gamerules`.
- **Rulesets:** each gametype comes with competitive settings; an admin
  changes only what differs in `rulesets/<gametype>.cfg`, and can offer
  variants of their own (`g_ruleset_variants`).

### Matches
- Warmup with every weapon the map places, ready-up, a countdown, a live
  match with the map reset, overtime, and intermission. A JSON record of
  each match (`g_matchlog_dir`; schema `q2game-match/0`).
- **Timeouts:** one per side per match (`time-out` / `time-in`).
- **Teams:** first-on captains, team locks with a roster (a dropped player
  can rejoin), `teamkick`, `readyteam`, and a random shuffle by vote.
- **Votes:** map, gametype, restart, time and score limits, powerups,
  friendly fire, ready-all, ready-team, shuffle; which are allowed, when,
  and how often are the server's choice.
- **Map rotation:** a list per gametype, optionally shuffled in passes.
- **Joining:** newcomers arrive watching with the menu open; players keep
  their place (and team) across map changes.
- **In-game admin:** `admin <password>` runs the match from a fixed list of
  commands (never `set` or `exec`), logged and announced, with a lockout
  after three wrong passwords.
- **Location callouts:** `loc` with MuffMode's tokens (`%l %h %a %w %m %n
  %N`) and community `.loc` files, or the nearest item with none.

### Interface
- The Tab menu: join, teams, votes, timeouts, match info.
- Scoreboards: FFA in columns (frags, deaths, frags per hour, time, ping),
  Duel's duellists and queue, ready marks and captains on team boards.
- A HUD status line (warmup, ready-up, votes, the countdown), a miniscore,
  frag messages, and a first-person view when watching someone (`eyecam`).

### Maps
- `.ent` override files (MuffMode's keys: per-gametype items, jump pads,
  small ammo); `sv saveents` writes a map's list to start from.
- Original Quake II's jump heights below height 0 (`g_classic_jumps`).
- `g_disable_items`: items, weapons or ammo that don't spawn on any map
  (MuffMode's `disable_<item>`), e.g. `"item_power_shield"`. Empty by
  default.

### Fixed from stock
- Tag crashed on the first frame of every map.
- A client still connecting could crash a TDM or CTF server with `team`.
- The water-jump probe could hang the server at zero or negative gravity.
- Stock CTF's admin election (any player voted in to stock's admin menu
  and `boot`) is replaced by the password.

### For mod authors
- Build your mod on this one without forking: your own gametypes and
  identity, with Axiom as a submodule. Guide: docs/downstream.md; a
  working example: `examples/jump`.

### Known issues
- Q2REX's built-in bots don't play a match until a human has joined
  (retail's rule).
