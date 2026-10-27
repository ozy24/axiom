// The Tab menu's hooks into id's code (each a marked one-line call).
#pragma once

struct edict_t;

// From Cmd_Inven_f (Tab), once an open menu or inventory has been closed: in
// deathmatch under a registered gametype, opens the menu and returns true.
// False elsewhere (single player, coop, Rogue's stock gamerules modes), and
// stock's inventory or CTF join menu shows as before.
bool Core_Menu_Inven(edict_t *ent);

// From the top of CTFOpenJoinMenu, which stock calls on connect in team
// modes, from Tab, `observer` and "Return to Main Menu": the same menu,
// under the same conditions; true if it opened.
bool Core_Menu_JoinMenu(edict_t *ent);
