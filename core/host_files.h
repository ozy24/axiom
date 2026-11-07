// Optional file access from the engine (docs/decisions/0011).
//
// game_import_t has no filesystem calls. An engine may offer them through
// gi.GetExtension (Q2RED's Q2RED_FILESYSTEM_API_V1); retail and Q2REX don't,
// so every caller has a fallback and nothing requires it.
#pragma once

#include <string>
#include <string_view>

// True when the engine gave the module file access. Asks once per load.
bool Core_HostFiles_Available();

// Replaces `game_path` (see Core_GamePath) under the writable game directory,
// where saves go, atomically. False when unavailable, refused or failed.
bool Core_HostFiles_Write(const std::string &game_path, std::string_view data);

// Reads `game_path` through the engine's search path (paks included) into
// `data`. False when unavailable or the file isn't there.
bool Core_HostFiles_Read(const std::string &game_path, std::string &data);

// A game path the extension accepts - relative, '/'-separated, under 64
// bytes, [A-Za-z0-9_.-], no empty or '.'-led component - built as
// "<dir>/<file>". Empty if `dir` or the result doesn't qualify. Pure.
std::string Core_GamePath(std::string_view dir, std::string_view file);

// A game data file, <dir>/<file>: through the engine's search path when it
// offers one (Core_HostFiles_Read), else the plain path
// <basedir>/<game>/<dir>/<file> from the server's working directory. `where`
// says which. False if neither has it. (core/host_files_gi.cpp)
bool Core_ReadGameFile(std::string_view dir, std::string_view file, std::string &text, std::string &where);
