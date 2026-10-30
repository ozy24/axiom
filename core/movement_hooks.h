// Classic jumps' hook into id's code (decision 0024; core/movement.h).
#pragma once

// From SpawnEntities, beside the air acceleration: classic jumps on for
// this level in deathmatch with g_classic_jumps 1 (the default), for the
// server's pmove and, through a configstring, the client's prediction.
// Single player and coop stay stock. A change applies from the next map.
void Core_Movement_Spawn();
