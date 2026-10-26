# 0010: Tag and DeathBall are kept, as registry gametypes over Rogue's code

**Decided:** 2026-10-26

The plan left "port Rogue's `gamerules` modes or drop them on purpose" open.
They are stock rerelease multiplayer modes, and dropping them would make a
drop-in Axiom server do less than stock. Keeping them costs almost nothing:
`gametypes/tag` and `gametypes/ball` are data entries that run as
`gamerules 2` / `3`, so Rogue's own `InitGameRules` installs their hooks and
Rogue's 22 direct `gamerules` checks keep working. No Rogue code moved.

Making Tag selectable exposed a stock crash (known id bug #1), now fixed and
covered by harness tier 4.

**Reopen if:** keeping them blocks a change the core gametypes need; then
retire them with a decision entry, not silently.
