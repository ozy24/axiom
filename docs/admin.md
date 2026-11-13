# Running a server

## Installing

The mod is one file: `game_x64.dll` (Windows) or `game_x64.so` (Linux).
Players need nothing: they join with their normal client.

- **Windows, the retail game or Q2REX:** back up `baseq2/game_x64.dll` and
  put the mod's in its place. Single player, coop and the mission packs
  keep working as stock, so the swap is safe for a listen server too. To
  go back, restore the backup.
- **Linux, Q2RED's dedicated server:** give it a folder of its own beside
  `baseq2` (`<basedir>/axiom/game_x64.so`) and start the server with
  `+set game axiom`. Maps, `.ent` and `.loc` files are still found in
  `baseq2`. The `.so` is built on Ubuntu 22.04 with its C++ runtime
  linked in, so it loads on any distribution with glibc 2.35 or later.

The server's console prints the version as the game loads
(`Axiom 0.1.0 (<build>)`), and `g_version` holds it.

The files below (`server.cfg`, `rulesets/`, `maps/`, `locs/`) go in the
same game folder as the module.

## Your files

Two kinds of file, and plain console commands in both (decision 0013).

| File | Holds |
|---|---|
| `server.cfg` | The server: name, slots, passwords, voting, and the mode it starts in |
| `rulesets/<gametype>.cfg` | Your changes to a gametype's gameplay: only what differs |

Nothing else is needed: every gametype works with no files at all.

### server.cfg

```
set hostname "My Server"
set deathmatch 1
set maxclients 16
set g_ruleset ffa         // the mode to start in: ffa, duel, tdm, ctf
map q2dm1
```

Leave gameplay settings (`timelimit`, `fraglimit`, warmup...) out of
`server.cfg`: they are replaced when the mode changes. They go in the
gametype's file.

## Changing a gametype

Each gametype comes with sensible settings (`sv rulesets` lists them). To
change some, create `rulesets/<gametype>.cfg` with just those:

```
// rulesets/tdm.cfg
set timelimit 15
set fraglimit 100
```

It applies every time TDM starts, by vote or by `sv ruleset tdm`, and
when the server starts in TDM (`set g_ruleset tdm`). Write settings as
`set name value`: at startup those apply before the first map spawns, and
anything else in the file runs only once the first player arrives.

A public FFA that starts as soon as two players are in, with no ready-up:

```
// rulesets/ffa.cfg
set match_ready 0
```

A tie when time runs out: Duel, TDM and CTF play 2-minute overtimes,
another each time it's still tied when one ends (the deciding score in CTF
is a capture); FFA goes to sudden death, the next frag wins. To change a
gametype's, set `match_overtime` in its cfg: `-1` sudden death, `N` N-minute
overtimes, `0` none (the match ends tied). `match_overtime_max` (minutes;
30 in every built-in, `0` never) ends a tie that has lasted that long into
overtime as a draw, so a tie nobody breaks can't hold the server on one map:

```
// rulesets/duel.cfg
set match_overtime -1      // sudden death instead of overtimes
set match_overtime_max 10  // still tied after 10 minutes: a draw
```

Timeouts: in Duel, TDM and CTF each side (a player, or a team) may stop a
match under way once, with `time-out` or the Tab menu: everything stands
still for 2 minutes, then a 10-second countdown back to play. The side that
called it can end it early with `time-in`. FFA has none: one player
shouldn't freeze a public server. To change them:

```
// rulesets/tdm.cfg
set match_timeout 60         // seconds; 0 turns timeouts off
set match_timeout_resume 5   // the countdown back to play
```

`match_minplayers` (2) counts bots, so one player and the server's bots
(`bot_minPlayers`) can start a match; a match always needs a human, so
bots alone stay in warmup, and a match whose last human leaves goes back
to warmup. For a bot-only server, `set match_allow_no_humans 1`.

## Adding a variant

A variant is another way to play a gametype: Instagib CTF, say, with
`rulesets/ctf_instagib.cfg`. None are built in (decision 0022).
To add your own, name the file `<gametype>_<name>.cfg`, write only how it
differs from the gametype, and list it in `server.cfg`:

```
// rulesets/ctf_instagib.cfg
set capturelimit 5
```

```
// server.cfg
set g_ruleset_variants "ctf_instagib"
```

Players then find it in the vote menu under Capture The Flag, as
"Instagib". A variant always starts from its gametype's settings, including
your `rulesets/ctf.cfg`.

## Switching modes

| | |
|---|---|
| `sv ruleset <name>` | switch: its settings apply at once, its gametype at the next map, so follow it with one (`map q2dm2`) |
| `sv rulesets` | list every mode and where its settings come from |
| `callvote gametype <name>` | players' way: a vote, which reloads the map |

Each switch starts clean: nothing a previous mode set carries over,
stock's gameplay switches included (`g_instagib`, `g_dm_weapons_stay`,
`g_no_armor`...). Four settings are the server's, not a mode's, and hold
across switches: `g_classic_jumps`, `g_teleporter_freeze`, `g_no_blood`
and `sv_gravity`. Set them in `server.cfg`.

## No blood

`set g_no_blood 1` (default 0) makes a game without blood: hits on players
and monsters show sparks instead of blood, and a gibbed body's pieces and
head are not drawn (metal pieces stay, without a blood trail). It changes
only what the server sends, so it works with stock clients, in deathmatch
and single player alike. Set it before playing, in `server.cfg` or the
console; it applies to every hit from then on.

## Joining

A player new to the server arrives spectating, with the menu open, and
chooses to play (`join`, or the menu); players stay in from map to map.

| | Default | |
|---|---|---|
| `g_auto_join` | 0 | 1: everyone goes straight into play (the smaller team) |

Set it in `server.cfg`. Details: docs/design/joining.md.

## Running a match from the console

| | |
|---|---|
| `sv readyall` | in a ready-up warmup: everyone playing is ready, so the countdown starts (as a passed `callvote readyall`) |
| `sv readyteam red\|blue` | the same for one team |
| `sv spectate <player>` | out of play, in any mode: a name as shown (any case, spaces and all) or a client number. On a team: off it and its roster, so a locked team keeps them out (a captain's `teamkick`). In Duel, a duellist in a match under way sends it back to warmup and the next in line steps up: no forfeit |
| `kick <player>` | the engine's own: off the server |
| `sv timeout` | stop the match under way now (two minutes, or `match_timeout`); needs no player's allowance |
| `sv timein` | end any timeout: the countdown back to play starts |
| `sv setteam <player> red\|blue` | onto a team, as if they had joined it: not past a lock (unlock it first) |
| `sv endmatch` | end the match under way now, as it stands: the intermission and its results |
| `sv resetmatch` | the match under way back to warmup, every score zeroed |
| `sv passvote` / `sv cancelvote` | decide the open vote: passed (it runs after the usual delay) or gone |
| `sv match` | the state, and everyone's score (and a timeout under way) |

Bots aren't moved: use `bot_count`.

## Match records

```
set g_matchlog_dir "matches"   // empty (the default): none written
```

Each finished match is written as one JSON file,
`<yyyymmdd-hhmmss>-<map>.json` (UTC): the gametype, ruleset, map, why it
ended, how long it was live, every player's score and team, the teams'
scores and the winner. The log says where (`Match record: ...`). A
relative directory goes in the game folder on Q2RED, and under the
server's working directory elsewhere.

The format is `"schema": "q2game-match/0"`: version 0, which may still
change before 1.0 (listed in CHANGELOG.md when it does).

## Admins in the game

Give trusted players a password and they can run a match from inside the
game, with no console access:

```
set admin_password "something long"   // in server.cfg; empty (the default): no admins
```

A player types `admin <password>` in the console (never in chat: chat goes
to everyone; quote a password with spaces) and is an admin until they leave; everyone is told. Then
`admin` lists the commands and `admin <command>` runs one:

| | |
|---|---|
| `match`, `teams`, `maplist`, `rulesets` | look: the state, the teams, the maps, the modes |
| `readyall`, `readyteam`, `lockteam`, `unlockteam`, `shuffle`, `setteam`, `spectate`, `timeout`, `timein`, `endmatch`, `resetmatch`, `callvote`, `passvote`, `cancelvote`, `ruleset` | as `sv <command>` above |
| `map <name>` | change map: only maps on `g_map_list` |
| `kick <player>` | off the server |

That is all an admin can do: never a `set`, an `exec` or anything else the
console can, so a leaked password spoils a match, not the server. Each
command is in the server log with who ran it, and players see everything
but the looks (`Alice (admin): kick Bob`).

- Three wrong passwords lock that player out of `admin` for 10 minutes
  (by their account, so reconnecting doesn't help); each wrong one is
  logged with their name, account and address.
- A new `admin_password` logs every admin out: the way to take it back.
- Keep it different from `rcon_password` (the server warns if they match).
- On a LAN the password crosses the network as plain text.

Details: docs/design/admin.md, decision 0026.

## Scoreboard

| Cvar | Default | What |
|---|---|---|
| `g_scoreboard` | 0 | 1: the mod's boards: FFA's columns (#, Frg, Dth, Player, FPH, Time, Ping), Duel's (the duellists, the queue), and spectators shown with 0 and ping on every board; every built-in ruleset sets 1. 0: stock's boards |

See docs/design/scoreboard.md.

## Team locks and captains

In TDM and CTF the first player on each team is its captain, and a captain
can `lockteam`, `unlockteam`, `teamkick <player>` and `readyteam`. A locked
team takes back its own players (one who dropped can rejoin) and nobody
else.

| | Default | |
|---|---|---|
| `match_warmup_weapons` | 1 | in warmup, everyone has every weapon the map places, with full ammo; 0 = stock's start |
| `match_lock` | 1 in `tdm` and `ctf` | lock both teams as the match goes live; 0 = only when a captain locks |

From the console: `sv lockteam` and `sv unlockteam` (both teams, or
`red` / `blue` for one), `sv teams` to see each team's lock, captain and players, and `sv shuffle`
to split the players into two new random teams (warmup only; players can
vote one with `callvote shuffle`, decision 0020). For a public
server, put `set match_lock 0` in `rulesets/tdm.cfg` and `rulesets/ctf.cfg`.
Details: docs/design/teams.md, docs/decisions/0016.

## Changing a map's items

A map's entities can be replaced without repacking it: put the whole
entity list in `maps/<map>.ent` and a deathmatch level spawns from it.
`sv saveents` writes the current map's list there as a starting point (it
never overwrites one). Each entity can carry keys that choose its modes:

```
{
"classname" "item_armor_combat"
"origin" "-320 -672 2"
"gametype" "ffa"           // only in FFA; "ffa tdm" for two
}
{
"classname" "item_power_screen"
"origin" "-320 -672 2"
"not_gametype" "ffa"       // everywhere but FFA
}
```

Also `notteam`, `notfree`, `powerups_on` and `powerups_off` (with the
powerups vote). MuffMode's maps and `.ent` files use the same keys and
work as they are. The log names the file it used (`Entities:
maps/q2dm1.ent`); a broken one is skipped with the reason. `set
g_entity_override_load 0` turns the files off. Details:
docs/design/entities.md.

To take an item off every map instead, list it in `g_disable_items`,
in the gametype's ruleset (it applies from the next map):

```
set g_disable_items "item_power_shield"      // as MuffMode's disable_item_power_shield
```

Classnames, separated by spaces; only items, weapons and ammo (never a
CTF flag). Empty by default: every item spawns, as stock.

## Location callouts

Players' `loc` binds (`cmd loc ENEMY - %l`, docs/players.md) name their
place from a map's `.loc` file: `locs/<map>.loc`, the community format
(MuffMode's and other mods' files work as they are):

```
// one place per line: x y z (eight times the map's coordinates), then its name
1024 -512 256 Red Armor
```

| | Default | |
|---|---|---|
| `g_loc` | 0 (every built-in ruleset sets 2) | 0: off. 1: the map's `.loc` file only. 2: the file, or with none, the nearest weapon, powerup or mega health ("upper RG") |
| `g_loc_dir` | `locs` | where the files are |

A file is read when its map loads (`Loc: 136 places from locs/q2dm1.loc`
in the log). Callouts are team chat in team games and use the chat flood
limits (`flood_msgs`, `flood_persecond`, `flood_waitdelay`). Details:
decision 0027.

## Jumps and teleporters

Original Quake II let players step and jump a little higher below height
0, and some maps' jumps need it (mm-mcoil, which is all below 0). The
rerelease lost it; the mod brings it back in deathmatch, as MuffMode does.
`set g_classic_jumps 0` plays stock's heights instead, from the next map.
Single player and coop are always stock. Decision 0024.

Teleporters stop a player dead for a moment, as stock. `set
g_teleporter_freeze 0` (MuffMode's name and behaviour) sends them out of
the exit, facing its way, at the speed they went in with: Quake III's feel,
and what Bloody Blastard ran under MuffMode. Deathmatch only.

## Voting and maps

| | Default | |
|---|---|---|
| `g_vote` | 1 | voting on |
| `g_vote_allow` | `map gametype restart timelimit scorelimit powerups friendlyfire readyall readyteam shuffle` | what players can call; remove a word to turn that vote off |
| `g_vote_midgame` | 1 | also during a match; 0 = warmup only |
| `g_map_list` | (empty) | the map rotation, and the maps a vote may pick |
| `g_map_list_shuffle` | 1 | play the rotation in a random order; 0 = as written |

`sv callvote <kind> [choice]` puts a vote to the players from the console
(the server casts no vote; the player limits don't apply).

Players can also vote on the time limit (1-60 minutes), the frag or
capture limit, powerups and, in team modes, friendly fire; and in a
ready-up warmup, to ready everyone (`readyall`) or their own team
(`readyteam`, voted on by that team alone). A voted value
lasts until the gametype changes, as if you had typed `set`; the next
gametype starts from its own file again. The ranges are fixed
(decision 0015).

`g_map_list` and `g_map_list_shuffle` are gameplay, so set them in a
gametype's file (`set g_map_list "q2dm1 q2dm2 q2dm6 q2dm8"` in
`rulesets/ffa.cfg`). The rest is `server.cfg`. Details:
docs/design/voting.md, docs/decisions/0014.

How the rotation runs:

- Each map in the list plays once per pass. Shuffled, every pass is in a
  new order, and a map never comes back until at least half the list has
  played since its last turn.
- The map the server starts on (`+map`, or the listen server's menu) need
  not be in the list: after it, the rotation starts.
- A voted map that is in the list carries the rotation on from its place.
- With no list, each map leads to its own next one, as stock does (and a
  vote offers q2dm1-8, or q2ctf1-5 in CTF).
- `maplist` (players) and `sv maplist` (console) show the rotation and
  what's next; so does the Tab menu's Match Info, and everyone sees
  "Next map: ..." as the match ends.
