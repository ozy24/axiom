# 0005: No plugin DLLs until the gametype API is proven

**Decided:** 2026-10-26

The engine loads one `game_x64` module; the module could load plugin DLLs itself.
That would need a stable binary interface across compilers and releases, a
Windows and a Linux build of every plugin, and save registration for plugin
callbacks. Plugins also can't reach the client's cgame.

Gametypes are compiled in. Variants are rulesets. Downstream mods build their
own complete module on top of the core. The gametype API is kept to plain
function tables and simple types, so a plugin loader stays possible.

**Reopen if:** the API has been stable through Phase 6 and there is real demand
for third-party gametypes.
