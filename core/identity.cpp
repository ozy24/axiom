// Pure part of the identity: no engine access, so it is unit-testable.
#include "core/identity.h"

#include "core/host.h"
#include "mod_identity.h"

// A plain, greppable identity stamp in the module's read-only data, so a built
// game_x64.dll/.so can be identified without loading it:
//   strings game_x64.so | grep mod-identity
// Short string literals elsewhere may be emitted as immediate stores, so this is
// the one place the identity is guaranteed to appear as text (the rename drill
// checks it). extern gives it external linkage so it is kept in the object.
extern const char core_identity_stamp[];
extern const char core_identity_stamp[] =
    "mod-identity: name=" MOD_NAME " id=" MOD_ID " version=" MOD_VERSION " build=" MOD_BUILD;

std::string Core_VersionString()
{
	// Reading the stamp keeps it referenced, so no linker drops it (MSVC /OPT:REF).
	volatile char keep = core_identity_stamp[0];
	(void)keep;
	return std::string(MOD_NAME) + " " + MOD_VERSION + " (" + MOD_BUILD + ")";
}

void Core_Identity_PrintBanner(Host &host)
{
	host.Print(Core_VersionString() + "\n");
}
