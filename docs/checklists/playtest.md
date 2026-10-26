# Playtest checklist (manual, in game)

The game tests prove the logic; this is for what only a person can judge:
how it feels, whether messages are readable, and that a real retail client
sees everything properly. Run it at each gameplay milestone (now: match flow,
Duel, HoonyMode). About 20 minutes; better with a second human, fine with a bot.

## Set up (once)

```bash
scripts/playtest/setup-local-rig.sh          # makes ~/q2playtest
~/q2playtest/run.sh                         # Duel vs one bot on q2dm1
```

In retail Quake II (normal launch), open the console and `connect 127.0.0.1:5073`.
Change modes from a terminal: `~/q2playtest/cmd.sh "sv ruleset hoony"` then
`~/q2playtest/cmd.sh "map q2dm1"`. `~/q2playtest/stop.sh` when done.

## Match flow and Duel (`sv ruleset duel`)

| # | Try | Expect | Note what felt off |
|---|---|---|---|
| 1 | Join | You're in warmup: free play, nothing counts | |
| 2 | Type `ready` in the console | "<your name> is ready." broadcast; countdown starts (the bot is always ready) | |
| 3 | Watch the countdown | "Match starts in 5…1", then "FIGHT!" — readable, sensible length? | |
| 4 | At FIGHT | Everyone respawned, items back where they were, score 0 | |
| 5 | `notready` during the countdown | Countdown cancelled, back to warmup | |
| 6 | Have a third client join (second retail instance or `cmd.sh "bot_count 2"`) | They wait as a spectator; "waits in the duel queue" | |
| 7 | Type `queue` | The order, with who's duelling and who's waiting | |
| 8 | As the queued spectator: can you watch the duel? (chase cam) | | |
| 9 | Play to the timelimit (or `cmd.sh "timelimit 1"` before `map`) | Intermission; console shows `Duel: … next: …`, loser to the back | |

## HoonyMode (`sv ruleset hoony`, then `map q2dm1`)

| # | Try | Expect | Note |
|---|---|---|---|
| 10 | Get a frag (or be fragged) | Round ends at once: both out of play, "Next round in 3…1" | |
| 11 | Next round starts | Both respawn together, items reset, scores kept | |
| 12 | Play to 7 | The deciding frag ends the match (no extra round) | |
| 13 | Overall | Is the round pause too long / too short? Does it feel like HoonyMode? | |

## Report

Tell Claude: the build (`strings ~/q2playtest/baseq2/game_x64.so | grep mod-identity`),
pass/fail per row and anything that felt wrong. Those become changes or new
harness checks.
