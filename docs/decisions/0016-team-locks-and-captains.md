# 0016: Team captains and one lock per team, with a roster; match_lock on in team rulesets

**Decided:** 2026-10-29 (Chris)

TDM and CTF get captains, team locks and a team kick (docs/design/teams.md).
Compared with MMREX's version first; kept its captain rules and commands,
dropped its machinery.

1. **One lock per team, plus a roster.** A locked team takes back the
   players it had: anyone who was on it while it was locked, unless kicked.
   A player who drops or spectates keeps their place; a newcomer is
   refused. MMREX keeps two lock layers and no roster, so a crashed player
   can't rejoin.
2. **`match_lock` locks both teams as the match goes live**, and after that
   it's an ordinary lock: the captain can `unlockteam` for a substitute and
   `lockteam` again (Chris). There's no separate automatic lock to keep in
   step with the cvar.
3. **On in the built-in `tdm` and `ctf` rulesets**, off as the compiled
   default (stock). "The mod caters for the competitive players by
   default" (Chris). A pub server sets `match_lock 0` in its
   `rulesets/tdm.cfg`.
4. **An emptied team unlocks itself.** A locked team with no humans left
   has nobody to unlock it, and the match needs both teams staffed to
   start again.
5. **Captains as MMREX's:** the first human on the team, then the human
   longest on it; never a bot; handed on with `captain <player>`. Kept by
   social ID, not entity.
6. **Team kick is in** (Chris): the captain moves a teammate to spectating
   and off the roster. Bots are the server's: never kicked, never locked out.
7. **Locks belong to the match**: warmup clears them, a new level starts
   without any, rounds never touch them (Clan Arena, next).
8. **Admins use `sv lockteam` / `sv unlockteam`** from the console; there's
   no in-game admin role.
9. **Duel is left out.** Its queue already decides who plays, and a
   duellist leaving mid-match is a forfeit. A reconnect grace for a
   dropped player belongs with timeouts.

Not taken from MMREX: team names, team mute, invitations and passwords,
coaching, and the alias spellings (`teamlock`, `teamcaptain`, ...).

**Reopen if:** players want a substitute let in without unlocking for
everyone (an invite), or a locked team's roster should outlive a map change.
