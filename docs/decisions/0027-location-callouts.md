# 0027: Location callouts (`loc`)

**Decided:** 2026-11-06 (Chris: "go ahead and implement loc as you would
recommend").

MuffMode had `cmd loc`. Its players' binds (`bind c "say_team NEED
WEAPON; cmd loc NEED WEAPON - %l"`) stopped working when BB moved to
Axiom, and they asked for them back. Retail sends chat through the lobby,
so the game never sees `say_team` text. A console command is the only way
for the server to fill in the caller's place and status on every engine.

**Kept as MuffMode had it**, so existing binds work unchanged:
- `loc <text>`. The text needs at least one token, so `loc` can't be used
  as a second chat channel.
- The tokens: `%l` / `%L` the place, as `[place]`; `%h` `H:87`; `%a`
  `A:50`, with power armor `A:50 P:80`; `%w` `RG:25`; `%m` the ammo;
  `%n` teammates in view; `%N` players in view.
- `.loc` files in the community format (`x y z name`, coordinates eight
  times the map's), at `baseq2/locs/<map>.loc`.
- With no file, the nearest weapon, powerup or mega health in the PVS,
  MuffMode's short names ("RG", "QUAD", "MEGA"), "upper" / "lower" for
  twins, then "water" or the map name.

**Changed:**
- **It goes out as chat does:** in team games to the caller's team, and
  spectators reach only spectators. Otherwise it goes to everyone. The
  bases are `CTFSay_Team`'s and Q2RED's chat (`({}): {}`, `{}: {}`), with
  the name sent as a whole argument so the client decodes its `##P`
  token. MuffMode baked the name into the text and sent a spectator's
  callout to everyone.
- **Stock's flood check** (`flood_msgs`, `flood_persecond`,
  `flood_waitdelay`). MuffMode had none, so a mouse-wheel bind could spam
  a team.
- **The place in sight:** of the eight nearest places, the nearest with a
  clear line from the eye. Only if none is clear, the nearest by
  distance, as MuffMode did. Distance alone named the room through a
  wall.
- **`%n` / `%N`:** only players in front (stock's `infront()` cone) and
  visible, alive and in the match. At most four names, then "+N". Real
  names, never tokens inside the text.
- **Twins:** "upper" only when every other of the same item is lower,
  "lower" when every other is higher; otherwise no prefix. MuffMode
  compared against whichever twin it found first.
- **Item and ammo names are short names from the classname** ("BLASTER",
  "SLUGS"), never the pickup name: that is a `$loc` key, which the client
  translates only as a whole argument.
- **One pass:** a place named "%h" isn't expanded again. The callout is
  cut to 150 characters, chat's line cap.
- **Loaded at map spawn** through `Core_ReadGameFile`, the `.ent`
  override's reader (decision 0011: Q2RED's search path, paks included,
  else the plain path), and logged: `Loc: N places from locs/q2dm1.loc`.

**Settings:**
- `g_loc`: 0 off (compiled, as stock has no `loc`), 1 the `.loc` file,
  2 the file or else the nearest item. Every built-in ruleset sets 2.
- `g_loc_dir`: default `locs`.
- Two cvars, as MuffMode had (`g_loc`, `g_loc_items`).
- Deathmatch only: single player and coop never see the command.

**Not added:** new tokens, colours, per-player settings, or an in-game
`.loc` editor.

core/loc.cpp is the logic (tests/test_loc.cpp); the adapter is
core/loc_register.cpp, with two marked hooks in id's code (SpawnEntities,
ClientCommand). Harness tier 27 checks it with simulated players.

**Reopen if:** players want plain `say_team ... %l` binds. Q2RED owns
chat (D-ENGINE-CHAT), so a Q2RED extension could hand the game each chat
line to expand before it is sent. That would work on Q2RED only;
`cmd loc` stays the route that works everywhere. Also reopen if admins
need to build `.loc` files for new maps in game (an `addloc`).
