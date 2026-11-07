// In-game admin's hooks into id's code and the other core adapters
// (core/admin.h, docs/design/admin.md).
#pragma once

struct edict_t;

// `admin <password>` and `admin <command>`, deathmatch only. True if handled.
// From ClientCommand, before id's chain.
bool Core_Admin_ClientCommand(edict_t *ent, const char *cmd);

// Every game frame (the match frame, and a timeout's frame): who is here, so
// an admin who has left stops being one; a new admin_password logs every
// admin out.
void Core_Admin_Frame();
