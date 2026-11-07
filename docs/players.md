# Playing

Nothing to install: join a server with your normal Quake II client. Type
commands in the console (`~`), or bind them to keys. The **Tab menu** does
the common ones: join or leave, pick a team, vote, call a timeout.

## Joining

You arrive watching, with the menu open (unless the server puts everyone
straight in).

| Command | |
|---|---|
| `join` | play: in a team game on the smaller team; in Duel, into the queue |
| `team red` / `team blue` | play on that team (TDM, CTF) |
| `spectate` | stop playing and watch |
| `queue` | Duel: who plays next |
| `eyecam` | while watching someone: first person or third (also `+use`) |

## Matches

A match starts with a **warmup**: play freely, nothing counts. On most
servers everyone types `ready` (`notready` takes it back); when all are
ready a countdown starts, the map resets and the match is **live**. Leaving
a Duel while it's live is a forfeit.

| Command | |
|---|---|
| `ready` / `notready` | ready up in warmup |
| `time-out` / `time-in` | stop a live match, once per side per match, where the server allows it (its Duel, TDM and CTF do unless changed: two minutes); `time-in` ends yours early. Not `timeout`: that's a client setting |
| `maplist` | the maps on this server's rotation |

## Votes

`callvote <what> [choice]`; everyone playing answers with `vote yes` or
`vote no` (or `yes` / `no`, or the menu).

| Vote | |
|---|---|
| `map <name>` | a map from the rotation |
| `gametype <name>` | `ffa`, `duel`, `tdm`, `ctf`, or a variant the server offers |
| `restart` | the map again, from warmup |
| `timelimit <minutes>` / `scorelimit <n>` | at once, no restart |
| `powerups on\|off`, `friendlyfire on\|off` | |
| `readyall` | everyone is ready |
| `readyteam` | your team is ready (your team votes) |
| `shuffle` | two new random teams (warmup) |

Servers choose which votes they allow; `callvote` alone lists them.

## Teams

The first player on each team is its **captain** (`*` on the scoreboard).

| Command | Who | |
|---|---|---|
| `captain` | anyone | who your captain is |
| `captain <player>` | captain | hand it to a teammate |
| `lockteam` / `unlockteam` | captain | keep others off your team; your own can always rejoin |
| `teamkick <player>` | captain | send a teammate to watch |
| `readyteam` | captain | ready your whole team |

Most servers lock both teams as the match goes live.

## Location callouts

`loc <text>` sends your teammates a message with your place and status filled
in (everyone, outside team games). Made for binds:

```
bind c "cmd loc NEED WEAPON - %l"
bind e "cmd loc STATUS %h %a %w %l"
```

`(Alice): STATUS H:87 A:50 RG:25 [Red Armor]`

| Token | |
|---|---|
| `%l` | where you are: the map's place names, or the nearest weapon or powerup ("upper RG") |
| `%h` / `%a` | health, armor (`A:50 P:80` with power armor) |
| `%w` / `%m` | your weapon and its ammo, your ammo type |
| `%n` / `%N` | teammates / players you can see |

## Admins

A server can give trusted players a password: `admin <password>`, then
`admin` lists what you may do (run the match, change the map, kick).
Never type the password in chat: chat goes to everyone.
