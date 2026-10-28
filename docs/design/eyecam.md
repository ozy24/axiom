# Eyecam

A spectator chasing a player sees through that player's eyes: their view,
gun, kick, screen blends, fov and underwater warp. `+use` or the `eyecam`
command flips to stock's third-person chase cam and back. Deathmatch only;
the default is first person.

## How it works

- Stock's chase cam (`g_chase.cpp`) still picks and follows the target. In
  `UpdateChaseCam`, once the target is known, the eyecam hook puts a
  first-person chaser at the target's eye and skips stock's third-person
  placement.
- At the end of `G_SetSpectatorStats`, the chaser's player state gets the
  target's gun, kick, blends and fov, so it shows what the target sees. The
  weapon icon is hidden there (`core/id_view*`).
- The target's own body would fill the view. It is marked `SVF_INSTANCED`,
  and `Entity_IsVisibleToPlayer` hides it from its first-person chasers
  only. Everyone else still sees it.
- Who chose third person is a per-slot bit (`EyeCamPrefs`, `core/eyecam.h`,
  unit-tested). It is cleared on disconnect, so the slot's next client
  starts in first person.

Code: `core/eyecam.h` (pure), `core/eyecam_register.cpp`, and marked
one-line hooks in `g_chase.cpp`, `g_cmds.cpp`, `p_hud.cpp`, `g_items.cpp` and
`p_client.cpp` (`core/eyecam_hooks.h` lists each).

