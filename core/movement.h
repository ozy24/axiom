// Classic jumps (decision 0024): original Quake II's extra height below
// height 0, which the rerelease lost and some maps' jumps need (mm-mcoil,
// all of it below 0). As MuffMode plays it: under z 0 a step is 20 units
// instead of 18 and a jump 274 instead of 270, and a jump's speed is
// rounded up. Pure logic, called from id's pmove (p_move.cpp), which the
// server and the client's prediction both run; the adapter is
// movement_register.cpp.
#pragma once

// The highest step pmove climbs or snaps down at height `z`.
float Core_Movement_StepSize(bool classic, float z);

// The upward speed a jump leaves at height `z` with upward speed `vz`.
float Core_Movement_JumpSpeed(bool classic, float vz, float z);
