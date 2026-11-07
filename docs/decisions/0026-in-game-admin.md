# 0026: In-game admin

**Decided:** 2026-11-02 (Chris, on the proposal below: "i will go with your
recommendations").

Running a match took the server console (`sv readyall`, `sv lockteam`,
`sv timeout`...). MMREX has `admin <password>` and an admin's commands; we
took its login and throttle, and kept the command list to the match.

**Decided:**
- **Off unless a password is set:** `admin_password`, empty by default
  (stock has none). `admin <password>` in the console; an admin until they
  leave the server (gone for over a minute: a map change under KEX
  reconnects everyone, and must not log them out). Everyone is told who
  became one.
- **Guessing is slow:** three wrong passwords lock that player out for ten
  minutes, by their account (social ID), so a reconnect doesn't reset it.
  Each wrong one is logged with name, account and address, never the guess.
  `admin <an admin command>` from a non-admin isn't a guess.
- **A leaked password can only spoil a match:** an admin runs a fixed list,
  the console's `sv` match commands plus `kick` and `map` (only maps on
  `g_map_list`), never a `set`, `exec`, `quit` or anything else the console
  can. The list is a table (`core/admin.cpp`); a test checks the console's
  own powers aren't on it.
- **Everything is on the record:** each admin command is logged with who ran
  it, and broadcast (`Alice (admin): kick Bob`) unless it only looks.
- **Taking it back:** a new `admin_password` logs every admin out.
- **Separate from rcon:** the server warns if the two passwords match.
- **One level:** no referee/full split until someone asks. Map and ruleset
  changes are in: they are what an admin is usually there for.
- **New console commands with it**, so the console and an admin have the
  same list: `sv setteam`, `sv endmatch`, `sv resetmatch`, `sv passvote`,
  `sv cancelvote`.

**Not closed:** on a LAN (direct UDP) the password crosses the network as
plain text; a password typed into chat by mistake goes to everyone (chat
goes through the lobby; the game never sees it): change it. Logging in by
social ID alone (no password) waits until it's checked that a client can't
claim someone else's ID.

core/admin.cpp is the logic (tests/test_admin.cpp); the adapter is
core/admin_register.cpp, with one marked hook in id's code (ClientCommand).
Harness tier 26 checks it with simulated players.

**Reopen if:** admins need levels; a password should be per admin; or the
social ID proves trustworthy enough for an admin list.
