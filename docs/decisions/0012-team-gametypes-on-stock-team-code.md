# 0012: TDM and CTF run on stock's team code; no team service

**Decided:** 2026-10-26

The plan was to move the team code out of `ctf/g_ctf.cpp` into a team service.
Not doing that. `tdm` and `ctf` are registry gametypes whose `retail_mode`
(`teamplay`, `ctf`) makes `Core_Gametype_PreInit` set the stock cvars, and id's
team code then runs exactly as stock: joining, balancing, spawns, flags,
scoring, the scoreboard. The match flow reads and resets team scores through
three marked accessors in `g_ctf.cpp` (`G_TeamCaptures`, `G_TeamFrags`,
`G_ResetTeamScores`).

Why: the extraction is a large edit to id's biggest mode file for no behaviour
change, and stock clients' HUD and scoreboard already depend on that code's
stats. Keeping it means TDM and CTF are stock-faithful by construction, and
harness tiers 4 and 8 check them on both engines.

CTF's own match and election system (`competition`, `matchtime`, the `admin`
and vote commands) is not removed. With `match_flow 0` it works as in
stock. With the match flow on, the flow owns the rules (`CheckDMRules`, and so
`CTFCheckRules`, isn't run) and Axiom's `ready` / `notready` are handled first;
a server using the match flow shouldn't also enable `competition`.

**Reopen if:** a gametype needs team behaviour stock's code can't give through
hooks (Freeze Tag's team elimination is the likely first), or CTF's election
system gets in the way of Phase 4's voting.
