# Running a server

## Installing

The mod is one file: `game_x64.dll` (Windows) or `game_x64.so` (Linux).
Releases ship the DLL; for Linux, build the `.so` from source (README).
Players need nothing: they join with their normal client.

- **Windows, the retail game or Q2REX:** back up `baseq2/game_x64.dll` and
  put the mod's in its place. Single player, coop and the mission packs
  keep working as stock, so the swap is safe for a listen server too. To
  go back, restore the backup.
- **Linux, Q2RED's dedicated server:** give it a folder of its own beside
  `baseq2` (`<basedir>/axiom/game_x64.so`) and start the server with
  `+set game axiom`. Maps, `.ent` and `.loc` files are still found in
  `baseq2`.

**Player limits:** the online lobby (the server browser, joining) has a
limit of its own, set with the engine's `kexmultiplayer maxplayers <n>`:
8 unless set, on the retail and Q2REX dedicated servers (Q2RED matches it
to `maxclients` by itself). Keep it equal to `maxclients` (the example
`server.cfg` does): a lower lobby limit, or one filled by players who
have left the game but not the lobby, turns people away while the server
has free slots. Run before the lobby exists, it prints "Lobby not
active"; the engine keeps the number and uses it when the lobby opens. On a game
hosted from the menu, type both in the console (`maxclients 32`,
`kexmultiplayer maxplayers 32`) before the map loads.

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
set match_timeout 60         // seconds. 0 turns timeouts off
set match_timeout_resume 5   // the countdown back to play
```

`match_minplayers` (2) counts bots, so one player and the server's bots
(`bot_minPlayers`) can start a match; a match always needs a human, so
bots alone stay in warmup, and a match whose last human leaves goes back
to warmup. For a bot-only server, `set match_allow_no_humans 1` in
`server.cfg` (it holds across mode switches).

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
`g_no_armor`, `g_allow_techs`, `g_damage_scale`, `g_start_items`...).
Some settings are the server's, not a mode's, and hold
across switches: `g_classic_jumps`, `g_teleporter_freeze`, `sv_gravity`,
`g_scoreboard`, `g_frag_messages`, `g_loc`, `g_hud_timer`,
`g_hud_miniscore`, `g_dm_crosshair_id`, `g_inactivity` and
`match_allow_no_humans`. Set them in `server.cfg`.

### Stock's gameplay settings

Stock Quake II's own switches, for a ruleset that wants them. A mode
switch puts every one back to the default below before the mode's file
runs, so a setting stays in the file (and the mode) that sets it: a
grapple turned on in `ctf.cfg` is gone in FFA.

| Setting | Default | What |
|---|---|---|
| `g_instagib` | 0 | 1: railguns that kill in one shot, nothing else |
| `g_infinite_ammo` | 0 | 1: ammo never runs out |
| `g_damage_scale` | 1 | all damage multiplied by this |
| `g_start_items` | (empty) | what players spawn with: `;`-separated classnames, each with an optional count (`"weapon_railgun;ammo_slugs 50"`) |
| `g_no_powerups` | 0 | 1: no Quad, DualFire, Double Damage, Invulnerability, Invisibility or Ground Zero spheres; adrenaline, the Rebreather, Environment Suit and the rest stay (MuffMode's setting). The `powerups` vote |
| `g_no_items` | 0 | stock's, wider: 1 also takes adrenaline, the Silencer, Rebreather, Environment Suit, IR goggles and the Doppelganger (every item stock calls a powerup). Health, armour, weapons and ammo stay |
| `g_no_armor` | 0 | 1: no armour on the map |
| `g_no_health` | 0 | 1: no health on the map |
| `g_no_mines` | 0 | 1: no prox mines or tesla (Ground Zero) |
| `g_no_nukes` | 0 | 1: no nukes (Ground Zero) |
| `g_no_spheres` | 0 | 1: no spheres (Ground Zero) |
| `g_disable_items` | (empty) | classnames that don't spawn on any map (`"item_power_shield"`); see "Changing a map's items" |
| `g_dm_weapons_stay` | 0 | 1: weapons stay when taken; you can't take one you already have |
| `g_weapon_respawn_time` | 30 | seconds before a taken weapon comes back |
| `g_dm_instant_items` | 1 | 1: a powerup is used the moment it's picked up; 0: it goes in the inventory |
| `g_dm_random_items` | 0 | 1: items are swapped for random others each map |
| `g_dm_no_quad_drop` | 0 | 1: a dying player doesn't drop their quad |
| `g_dm_no_quadfire_drop` | 0 | 1: nor their DualFire (The Reckoning) |
| `g_dm_no_stack_double` | 0 | 1: quad and double damage don't multiply together |
| `g_dm_strong_mines` | 0 | 1: prox mines last until they go off |
| `g_dm_no_fall_damage` | 0 | 1: no falling damage |
| `g_dm_spawn_farthest` | 1 | 1: respawn at the spawn point farthest from others |
| `g_dm_force_respawn` | 0 | 1: the dead respawn by themselves at `g_dm_force_respawn_time`, so nobody can stay dead while the clock runs out; 0: only by pressing fire. Every built-in ruleset sets 1 (decision 0030) |
| `g_dm_force_respawn_time` | 0 | with forced respawn, seconds dead before it (at least 1). Pressing fire respawns after 1 second either way, as on MuffMode. Every built-in ruleset sets 3 |
| `g_dm_same_level` | 0 | 1: the level's end replays the same map (with no `g_map_list`) |
| `g_dm_allow_exit` | 0 | 1: a map's exit ends the level; 0: touching it kills you |
| `g_instant_weapon_switch` | 0 | 1: weapons switch at once |
| `g_quick_weapon_switch` | 1 | 1: the rerelease's faster switch animation; 0: the original's |
| `g_disable_player_collision` | 0 | 1: players pass through each other |
| `g_allow_techs` | auto | the runes (Resistance, Strength, Haste, AutoDoc): auto = CTF only, 1 = every mode, 0 = none |
| `g_allow_grapple` | auto | the grappling hook: auto = CTF only (unless the map turns it off), 1 = every mode, 0 = none |
| `g_grapple_damage` | 10 | the grapple's damage |
| `g_friendly_fire` | 0 | TDM, CTF: 1 = teammates can hurt each other. Also the `friendlyfire` vote |
| `g_teamplay_armor_protect` | 0 | TDM, CTF: 1 = a teammate's damage skips armour |
| `g_teamplay_force_join` | 0 | TDM, CTF: 1 = players don't pick a team; they go on the smaller one |

The match settings (`match_*`), the limits and the map list are reset
the same way; they are described above.

## Joining

A player new to the server arrives spectating, with the menu open, and
chooses to play (`join`, or the menu); players stay in from map to map.

| | Default | |
|---|---|---|
| `g_auto_join` | 0 | 1: everyone goes straight into play (the smaller team) |
| `g_inactivity` | 120 | seconds a playing human may go without input (no button, movement or turn) before they are moved to spectate, warned 10 seconds before; at least 15. 0: never. Bots, spectators, timeouts and the intermission don't count. In a live Duel the move is a forfeit, as `spectate` is: a timeout is the way to take a break |

Set them in `server.cfg`. Details: docs/design/joining.md.

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
set admin_password "something long"   // in server.cfg. Empty (the default): no admins
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

## Scoreboard and HUD

| Cvar | Default | What |
|---|---|---|
| `g_dm_crosshair_id` | 1 | TDM, CTF: a teammate's name under the crosshair. 0: none (MuffMode's name) |
| `g_hud_timer` | 1 | The match clock on the HUD, bottom left of centre. 0: none (the match still keeps time) |
| `g_hud_miniscore` | 1 | FFA and Duel: the top two players' faces and scores, bottom right, while the match is in play. 0: none |
| `g_scoreboard` | 1 | The mod's boards: FFA's columns (Frg, Dth, Player, FPH, Time, Ping), Duel's (the duellists, the queue), and spectators shown with 0 and ping on every board. 0: stock's boards |
| `g_frag_messages` | 1 | On each frag, "You fragged ..." to the killer (and your place while live), "Fragged by ..." to the victim. 0: none |

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
| `g_loc` | 2 | 0: off. 1: the map's `.loc` file only. 2: the file, or with none, the nearest weapon, powerup or mega health ("upper RG") |
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
| `g_map_list` | (empty) | the map rotation, and the maps a vote may pick: names as in `map`, a pack's maps with a slash (`q64/dm3`; `q64\dm3` works too). A word that isn't a map name is left out, and the console says so |
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
