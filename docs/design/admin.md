# In-game admin

Decision 0026. The admin's side is in docs/admin.md ("Admins in the game").

## How it fits

- **Logic** (`core/admin.cpp`, tested in `tests/test_admin.cpp`): `Admins`
  holds who is an admin (by player key, `Core_PlayerKey`) and each key's
  wrong guesses. Times are a real-time clock, not level time: a timeout
  stops level time, and a `map` starts it again from zero. The command
  table (`Core_Admin_Commands`) is the whole of what an admin can do.
- **Adapter** (`core/admin_register.cpp`): `admin` from `ClientCommand`
  (one marked call), the cvars, and `Core_Admin_Frame`, from the match
  frame and a timeout's frame.

## Running a command

An admin's command is the console's own: `admin lockteam red` lines up with
`sv lockteam red` (`gi.argv(1)` on), so the adapter calls `ServerCommand()`
after checking the name is on the table. While it runs, `gi.Com_Print` is
swapped for one that also sends each line to the admin, with `sv ` in usage
lines turned into `admin `. So a new `sv` command is an admin command by
adding a row to the table, with nothing else to write.

`kick` and `map` aren't `sv` commands: the adapter finds the player (or
checks the map is on `g_map_list`) and queues the engine's `kick <slot>` /
`map <name>` for the next game frame, as a passed vote does. Run from inside
the client's own command, a `map` stranded the reconnecting players
(simulated clients, 2026-11-02).

## Staying an admin

`Core_Admin_Frame` passes the keys of every client here to
`Admins::Present`: an admin gone for over a minute stops being one (a slot
key, `slot:<n>`, at once: it names a slot, not a person). The state is kept
in the internal cvar `g_admin_memory` (`Encode`/`Decode`), because `map`
reloads the module; tier 26 changes map with `admin map` and Alice is still
an admin after it.

`admin_password` as last seen is kept too: a different value logs every
admin out.
