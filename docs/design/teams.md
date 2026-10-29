# Team locks and captains

Status: done (decision 0016). Change this page in the same PR as the code.

In TDM and CTF each team has a captain, and a team can be locked so nobody
new joins it. Locked teams keep pickups and scrims to the players who
started them. Console commands are the interface; the Tab menu runs the
same code. Pure logic: `core/teams.{h,cpp}` (tests: `tests/test_teams.cpp`);
engine side: `core/teams_register.cpp`; harness: tier 17.

## Commands

| Command | Who | Does |
|---|---|---|
| `captain` | anyone on a team | says who the captain is |
| `captain <player>` | captain | hands the captaincy to a human teammate |
| `lockteam` / `unlockteam` | captain | locks or unlocks their own team |
| `teamkick <player>` | captain | moves a teammate to spectating and off the team's roster |
| `readyteam` | captain | readies every teammate (in ready-up) |
| `sv lockteam red\|blue` / `sv unlockteam red\|blue` | server console | either team |
| `sv teams` | server console | each team's lock, captain and roster |

`<player>` is a name as shown (any case) or a client number. The name is the
whole argument, so a name with spaces needs no quotes.

## Rules

- **Captain:** the first human on a team. When the captain leaves the team
  (disconnects, spectates, switches), the human who has been on it longest
  takes over. Bots are never captain. A captain who reconnects doesn't get
  it back by themselves; the new captain can hand it back.
- **Locked team:** only its **roster** may join. The roster is the humans on
  the team when it locked, plus any human who is on it while it's locked
  (an admin moved them, a captain unlocked and relocked). Leaving or
  dropping keeps your place, so a player whose game crashes can rejoin.
  `teamkick` takes you off it. Unlocking clears the roster.
- **`match_lock 1`:** both teams lock as the match goes live (countdown to
  LIVE). After that it's an ordinary lock: the captain can unlock it to
  let a substitute in, then lock it again.
- **Locks belong to the match.** The match back in warmup unlocks both
  teams; a new level starts with no locks and no captains (players come
  back on their team, docs/design/joining.md, and the first back on each
  is captain). Rounds and overtime never change them.
- **An emptied team unlocks itself** when its last human leaves:
  otherwise nobody could join it, and the match could never start again.
- **Not locked:** spectating, chasing, bots (the server places them), and
  `sv` commands. With `g_teamplay_force_join 1` the server assigns teams:
  a lock still refuses `team` and the menu, and `teamkick` is refused,
  as there's nowhere to kick to.
- Refusals go to the player who asked, never to everyone.

## Cvar

| Cvar | Default | Meaning |
|---|---|---|
| `match_lock` | `0` | 1: both teams lock as the match goes live. The built-in `tdm` and `ctf` rulesets set 1 |

## What players see

- `RED is locked.` / `RED is unlocked.` to everyone; `Alice is captain of RED.`
  when the captaincy changes; `RED is locked.` to a player refused.
- The Tab menu: a locked team reads "Join Red [LOCKED]", and is skipped
  if you aren't on its roster. Match Info shows each team's captain and lock. A captain has
  a **Team** page: Lock or Unlock Team, Ready Team in ready-up, and their
  teammates; each opens Make Captain and Kick from Team.
- The scoreboard: a `*` left of each captain's name (as MMREX draws it).
- The server log: `Teams: RED locked`, `Teams: RED captain Alice`, and so on.

## Clan Arena

Clan Arena is a TDM ruleset (next). Locks fit it unchanged: they last for
the whole match, not a round. Joining is the lock's question; when you
spawn is the round's. A kick, a disconnect or a switch that leaves a team
with nobody alive must end the round, so CA's round end should count the
players alive, not frags.

## Implementation split

- `core/teams.{h,cpp}`: pure logic over a list of clients (key, team, bot)
  each frame; keys outlive a connection (`Core_PlayerKey`, a human's
  social ID). Returns events for the adapter to announce.
- `core/teams_register.cpp`: called from `Core_Match_Frame`. Watches the
  match flow for warmup and LIVE, announces, and handles the commands.
  Marked one-line hooks: `CTFTeam_f` and `CTFJoinTeam` (`rerelease/ctf/g_ctf.cpp`)
  ask `Core_Teams_MayJoin`; `ClientCommand` and `ServerCommand` pass the
  commands on.

## Compared with MMREX

MMREX (`match/captain.cpp`) has the same captain rules and commands. It
keeps two lock layers (a captain's and `g_match_lock`'s, kept in step with
the cvar's change count) where Axiom has one; it has no roster, so a player
who drops can't rejoin a locked team (its ghost codes cover some of that);
and a locked team that empties stays locked. Its captain is an entity
pointer, so a map change has to clear it by hand. It has four spellings
per command.
