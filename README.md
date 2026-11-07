# Axiom

A Quake II Rerelease multiplayer mod: **Free For All, Duel, Team
Deathmatch and Capture The Flag**, done properly, with the match handling
competitive and public servers need. It is a drop-in `game_x64.dll`: single
player, coop and the mission packs keep working exactly as stock, and
players join with their normal client.

- **Matches:** warmup, ready-up, countdown, overtime, timeouts, match
  records.
- **Teams:** captains, locks, shuffles, ready-team.
- **Votes and rotation:** a small fixed set of votes, map lists per
  gametype.
- **Admin:** rulesets as plain `.cfg` files changing only what differs;
  in-game admin by password.
- **Interface:** the Tab menu, column scoreboards, a HUD status line,
  location callouts.
- **A base for other mods:** build your gametype on it without forking.

| | |
|---|---|
| Playing | [docs/players.md](docs/players.md) |
| Running a server | [docs/admin.md](docs/admin.md) |
| Building a mod on it | [docs/downstream.md](docs/downstream.md) |
| What changed | [CHANGELOG.md](CHANGELOG.md) |

## Building

```bash
cmake --preset linux-release && cmake --build --preset linux-release
# build/linux-release/game_x64.so
```

Windows: `cmake --preset windows-msvc-release` from a Visual Studio
developer prompt. Released builds come from CI (MSVC for Windows, GCC 12 on
Ubuntu 22.04 for Linux).

## Contributing

Start with [AGENTS.md](AGENTS.md) (the rules, the checks and the test
harness), then [STATUS.md](STATUS.md). Settled questions are in
[docs/decisions/](docs/decisions/).

Licensed under the GPL, version 2: see `LICENSE` and
`THIRD_PARTY_NOTICES.md`. Built on id Software's
[Quake II Rerelease game source](https://github.com/id-Software/quake2-rerelease-dll);
id's original README is `docs/upstream/id-README.md`.
