# 0004: Public names are neutral; the mod name lives in one place

**Decided:** 2026-10-26

The working name may change. Cvars and commands end up in server configs, so a
mod prefix (`ax_`) would make a rename break every server. Public names use
neutral prefixes (`g_*`, `match_*`, `sv_*`). The mod's name appears only in
display strings and the data folder, both read from `mod.json` (Phase 1c), which
downstream mods override with their own.

**Reopen if:** never; a downstream mod that wants a prefix can add its own.
