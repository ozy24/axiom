/*
 * game_exports_posix.cpp - the module's C entry point on POSIX.
 *
 * Upstream's Q2GAME_API expands to `extern "C" __declspec(dllexport)` only when
 * KEX_Q2GAME_EXPORTS is defined, and to NOTHING otherwise (game.h). Nothing
 * else ever defined it, because the module has only ever been built for
 * Windows - so on Linux `GetGameAPI` is an ordinary C++ function and the symbol
 * carries its mangled name, which `Sys_GetGameAPI` cannot resolve.
 *
 * Defining KEX_Q2GAME_EXPORTS is not the fix: __declspec is MSVC's, and GCC and
 * Clang reject it on an ELF target. So this file exports the C name itself. The
 * asm label is what keeps the two from colliding: C++ sees a distinctly named
 * wrapper, the linker sees plain "GetGameAPI".
 *
 * Adapted from Q2RED's game-rerelease/q2red_game_exports.cpp (same author).
 */

#include "g_local.h"

/* g_local.h declares the game's own entry point only through Q2GAME_API, which
 * is empty here, so it is not visible. Declared explicitly, matching
 * g_main.cpp's definition exactly. */
game_export_t *GetGameAPI(game_import_t *import);

extern "C" game_export_t *posix_game_get_game_api(
    game_import_t *import) asm("GetGameAPI");

extern "C" __attribute__((visibility("default")))
game_export_t *posix_game_get_game_api(game_import_t *import)
{
    return GetGameAPI(import);
}
