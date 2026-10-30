# Playtest checklist (manual, in game)

The game tests prove the logic; this is for what only a person can judge:
how it feels, whether messages are readable, and that a real retail client
sees everything properly. Run it at each gameplay milestone (now: match flow,
Duel, HoonyMode, TDM, CTF). About 20 minutes; better with a second human, fine with a bot.

## Set up (once)

```bash
scripts/playtest/setup-local-rig.sh          # makes ~/q2playtest
~/q2playtest/run.sh                         # FFA with a bot on q2dm1
```

In retail Quake II (normal launch), open the console and `connect 127.0.0.1:5073`.
Change modes from a terminal: `~/q2playtest/cmd.sh "sv ruleset hoony"` then
`~/q2playtest/cmd.sh "map q2dm1"`. `~/q2playtest/stop.sh` when done.

## Match flow and Duel (`sv ruleset duel`)

| # | Try | Expect | Note what felt off |
|---|---|---|---|
| 1 | Join | You're in warmup: free play, nothing counts | |
| 2 | Type `ready` in the console | "<your name> is ready." broadcast; countdown starts (the bot is always ready) | |
| 3 | Watch the countdown | "Match starts in 10…1", then "FIGHT!" — readable, sensible length? | |
| 4 | At FIGHT | Everyone respawned, items back where they were, score 0 | |
| 5 | `notready` during the countdown | Countdown cancelled, back to warmup | |
| 6 | Have a third client join (second retail instance or `cmd.sh "bot_count 2"`) | They wait as a spectator; "waits in the duel queue" | |
| 7 | Type `queue` | The order, with who's duelling and who's waiting | |
| 8 | As the queued spectator: can you watch the duel? (chase cam) | | |
| 9 | Play to the timelimit (or `cmd.sh "timelimit 1"` before `map`) | Intermission; console shows `Duel: … next: …`, loser to the back | |

## HoonyMode (`sv ruleset hoony`, then `map q2dm1`)

| # | Try | Expect | Note |
|---|---|---|---|
| 10 | Get a frag (or be fragged) | The winner can still move; the loser stays dead about 2 s with the kill message | |
| 11 | Next round starts | Both respawn together, frozen (can look, can't move or fire), "Next round in 5…1", FIGHT!; items reset, scores kept, the bot visible | |
| 12 | Play to the fraglimit (5; `fraglimit 3` for a short test) | The deciding frag ends the match (no extra round) | |
| 13 | Overall | Are the 2 s pause and 5 s frozen countdown right? Does it feel like HoonyMode? | |

## TDM and CTF (`sv ruleset tdm` / `sv ruleset ctf`, then `map q2dm1` / `map q2ctf1`)

Add bots for the other team: `cmd.sh "bot_count 3"`.

| # | Try | Expect | Note |
|---|---|---|---|
| 14 | Join, pick a team (menu or `team red`) | Stock team menu and HUD; warmup until both teams have someone | |
| 15 | `ready` | Countdown, then FIGHT!: team scores zero, everyone respawns (CTF: both flags at base) | |
| 16 | CTF: take the enemy flag during warmup, then go live | The flag goes home at FIGHT!, you respawn without it | |
| 17 | CTF: capture | Capture counts on the HUD; at `capturelimit` (8) the match ends | |
| 18 | `sv ruleset ctf`, `set match_flow 0`, `set competition 1`, `map q2ctf1`; then `ready` | Stock CTF's own match ready-up answers (not "There is no match warmup") | |

## Report

Results go in `docs/checklists/results/<date>-playtest.md` (a copy of the
rows tested, with notes), so this file stays a clean template.

Tell Claude: the build (`strings ~/q2playtest/baseq2/game_x64.so | grep mod-identity`),
pass/fail per row and anything that felt wrong. Those become changes or new
harness checks.
