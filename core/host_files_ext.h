// The engine's optional file-access extension, as the module sees it.
//
// A copy of Q2RED's src/include/quake2ex_game_filesystem_extension.h (ledger
// A62): the name and the frozen v1 layout. Never edit it here; a new version
// gets a new name and struct. docs/decisions/0011-optional-host-file-access.md.
#pragma once

#include <cstddef>
#include <cstdint>

#define Q2RED_FILESYSTEM_EXTENSION_V1 "Q2RED_FILESYSTEM_API_V1"

typedef struct q2red_filesystem_api_v1_s
{
	uint32_t version;     // 1
	uint32_t struct_size; // sizeof(q2red_filesystem_api_v1_t)
	int64_t (*LoadFile)(const char *path, void **buffer);
	void (*FreeFile)(void *buffer);
	int32_t (*WriteFile)(const char *path, const void *data, uint64_t length);
} q2red_filesystem_api_v1_t;

static_assert(offsetof(q2red_filesystem_api_v1_t, LoadFile) == 8u, "frozen layout");
static_assert(offsetof(q2red_filesystem_api_v1_t, FreeFile) == 16u, "frozen layout");
static_assert(offsetof(q2red_filesystem_api_v1_t, WriteFile) == 24u, "frozen layout");
static_assert(sizeof(q2red_filesystem_api_v1_t) == 32u, "frozen layout");
