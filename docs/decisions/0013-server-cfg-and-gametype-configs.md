# 0013: server.cfg for the server, a config per gametype for gameplay

**Decided:** 2026-10-27 (Chris)

**Ease of administration comes first.** Chris stopped developing for
MuffMode when its "factories" arrived. They brought a custom file format,
`inherit` chains, hidden `_base_*` entries, a 220-cvar allow-list, 71
definitions and look-alike presets that its own docs call confusing. Axiom
keeps the model MuffMode had before them, a cfg per gametype:

1. **`server.cfg`** holds what doesn't depend on the gametype: `hostname`,
   `maxclients`, passwords, voting. It runs once, and a gametype switch
   never touches it. It picks the starting mode with `set g_ruleset <name>`.
2. **The gametype's config** holds its gameplay: warmup, ready-up, limits,
   overtime. Each gametype has compiled-in defaults (the built-in rulesets
   `ffa`, `duel`, `tdm`, `ctf`), so the module works with no files.
   `rulesets/<gametype>.cfg`, written by the admin, overrides them and holds
   only what changes.
3. **A variant** (a different way to play a gametype: HoonyMode, later Clan
   Arena or Instagib CTF) applies on top of its gametype's config. A built-in
   variant states only its differences (`hoony`: rounds, the suicide point,
   first to 5). An admin's own variant is `rulesets/<gametype>_<name>.cfg`,
   offered for voting by listing it in `g_ruleset_variants`.

Every switch (`sv ruleset`, a vote, a new `g_ruleset` at the next map) runs
the same steps:

1. Reset every gameplay cvar (`core_gameplay_cvars`) to its compiled default.
2. Apply the gametype's built-in settings, then `exec rulesets/<gametype>.cfg`.
3. For a variant, apply its built-in settings (if it has any), then
   `exec rulesets/<variant>.cfg`.
4. Load the map.

Because every switch starts from the same defaults, nothing one mode sets
can leak into the next. No values are stored and nothing is restored.

**What admins never see:** the list of gameplay cvars, the steps, or any
syntax beyond `set` and `exec`. There is no inheritance syntax, no
allow-list, no hidden entries and no file format of our own.

**Costs, accepted:**
- Gameplay cvars set in `server.cfg` are replaced on the first switch.
  Gameplay belongs in the gametype's cfg, and the docs and example say so.
- The module can't check whether a file exists (decision 0003), so a
  missing `rulesets/<gametype>.cfg` costs a harmless "couldn't exec" line in
  the server console on each switch. Players see nothing.
- A cvar an admin's cfg sets that isn't in `core_gameplay_cvars` isn't
  reset, so it can leak. Gameplay cvars join the list as features add them.
- Starting in a mode from `server.cfg`: `sv` commands don't exist before
  the game is loaded, so as it loads the module sets the mode's settings
  itself, and the `set` lines of its files (following their `exec`s), so
  the first map spawns in it. The engine holds the queued switch until a
  player arrives, then runs it whole, so a file's other commands (a bare
  `fraglimit 30`) wait for that first player. (2026-10-30: the files used
  to wait too, and BB's first map after a restart spawned without them.)

**Reopen if:** admins find any step of this surprising. The measure is what
an admin has to learn and write, not what the code can do.
