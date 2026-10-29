# Running a server

Two kinds of file, and plain console commands in both (decision 0013).

| File | Holds |
|---|---|
| `server.cfg` | The server: name, slots, passwords, voting, and the mode it starts in |
| `rulesets/<gametype>.cfg` | Your changes to a gametype's gameplay: only what differs |

Nothing else is needed: every gametype works with no files at all.

## server.cfg

```
set hostname "My Server"
set deathmatch 1
set maxclients 16
set g_ruleset ffa         // the mode to start in: ffa, duel, hoony, tdm, ctf
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

It applies every time TDM starts, by vote or by `sv ruleset tdm`.

## Adding a variant

A variant is another way to play a gametype. HoonyMode (Duel) is built in.
To add your own, name the file `<gametype>_<name>.cfg`, write only how it
differs from the gametype, and list it in `server.cfg`:

```
// rulesets/ctf_instagib.cfg
set capturelimit 5
```

```
// server.cfg
set g_rulesets "ctf_instagib"
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

Each switch starts clean: nothing a previous mode set carries over.

## Joining

A player new to the server arrives spectating, with the menu open, and
chooses to play (`join`, or the menu); players stay in from map to map.

| | Default | |
|---|---|---|
| `g_auto_join` | 0 | 1: everyone goes straight into play (the smaller team) |

Set it in `server.cfg`. Details: docs/design/joining.md.

## Team locks and captains

In TDM and CTF the first player on each team is its captain, and a captain
can `lockteam`, `unlockteam`, `teamkick <player>` and `readyteam`. A locked
team takes back its own players (one who dropped can rejoin) and nobody
else.

| | Default | |
|---|---|---|
| `match_lock` | 1 in `tdm` and `ctf` | lock both teams as the match goes live; 0 = only when a captain locks |

From the console: `sv lockteam red|blue`, `sv unlockteam red|blue`, and
`sv teams` to see each team's lock, captain and players. For a public
server, put `set match_lock 0` in `rulesets/tdm.cfg` and `rulesets/ctf.cfg`.
Details: docs/design/teams.md, docs/decisions/0016.

## Voting and maps

| | Default | |
|---|---|---|
| `g_vote` | 1 | voting on |
| `g_vote_allow` | `map gametype restart timelimit scorelimit powerups friendlyfire readyall readyteam` | what players can call; remove a word to turn that vote off |
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
