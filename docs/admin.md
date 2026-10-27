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
set g_ruleset duel        // the mode to start in: ffa, duel, hoony, tdm, ctf
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
| `sv ruleset <name>` | switch; it takes effect at the next map (`map q2dm2`) |
| `sv rulesets` | list every mode and where its settings come from |
| `callvote gametype <name>` | players' way: a vote, which reloads the map |

Each switch starts clean: nothing a previous mode set carries over.

## Voting and maps

| | Default | |
|---|---|---|
| `g_vote` | 1 | voting on |
| `g_vote_allow` | `map gametype restart` | what players can call |
| `g_vote_midgame` | 1 | also during a match; 0 = warmup only |
| `g_map_list` | (stock maps) | maps a vote may pick; empty = q2dm1-8, or q2ctf1-5 in CTF |

`g_map_list` is gameplay, so set it in a gametype's file. The rest is
`server.cfg`. Details: docs/design/voting.md.
