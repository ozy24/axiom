# 0020: A random team shuffle, voted in warmup; no balance vote

**Decided:** 2026-10-29 (Chris)

TDM and CTF get `callvote shuffle` (and `sv shuffle` for the console);
docs/design/voting.md has the rules.

1. **Random, not ranked.** The players on teams are split at random into
   two teams whose sizes differ by at most one. MMREX sorts by rating and
   alternates (1st, 3rd, 5th to red), which gives lopsided teams; dealing by
   match score was considered and turned down (Chris chose random).
2. **Never the same groups back.** With three or more players, a split
   equal to the teams now (in either colour) is drawn again, so a passed
   vote always changes something. Fewer than three players is refused:
   there is no other split.
3. **Warmup only** (or the countdown): the teams are set before the match
   goes live, and team locks (`match_lock`) take over from there. A shuffle
   mid-match would mean a restart; players who want that vote `restart`
   first. With the match flow off there's no warmup, so no shuffle.
4. **It unlocks both teams** (a lock's roster is the old team) and respawns
   everyone on a team, as going LIVE does, not through stock's change of
   team (a suicide each, refused by a lock). The captains settle as usual:
   a captain who moved team hands over to the longest on their old one.
5. **No `balance` vote.** Balancing needs a rating system and we have none
   (Chris). Reopen this with ratings.
