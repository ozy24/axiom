# 0004: Public names are neutral; the mod name lives in one place

**Decided:** 2026-10-26

The working name may change. Cvars and commands end up in server configs, so a
mod prefix (`ax_`) would make a rename break every server. Public names use
neutral prefixes (`g_*`, `match_*`, `sv_*`). The mod's name appears only in
display strings and the data folder, both read from `mod.json` (Phase 1c), which
downstream mods override with their own.

**Reopen if:** never; a downstream mod that wants a prefix can add its own.

## Amended 2026-10-29 (Chris): where names come from

1. **Stock's name when stock has the setting.** We set `g_friendly_fire`,
   `g_no_items`, `g_map_list`, `g_dm_no_fall_damage` and the like; we don't
   add a copy under our own prefix.
2. **Otherwise a neutral name of our own**: `g_*` for server-wide features
   (votes, joining, the scoreboard), `match_*` for the match flow (warmup,
   ready-up, countdown, overtime, rounds, team locks).
3. **Never another mod's name just to match it** (MuffMode, MMREX), unless
   the behaviour is the same. A familiar name that behaves differently
   (MuffMode's `minplayers` ends a match; ours never does) is worse than a
   new one, and their configs don't carry over anyway.
4. **Names say what they do, and differ by more than a letter.** Renamed
   on the same day, before any server but the playtest rig ran them:
   `match_warmup` → `match_flow` (it turns on the whole match flow, not just
   warmup) and `g_rulesets` → `g_ruleset_variants` (one letter from
   `g_ruleset`).
5. **Internal state is write-protected.** Cvars the module keeps only to
   survive a module reload (`g_join_memory`, `g_map_order`,
   `g_ruleset_applied`) are `CVAR_NOSET`: the console can't set them, only
   the module (`cvar_forceset`). Harness tier 15 checks it.

