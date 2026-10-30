// Classic jumps' numbers (core/movement.h).
#include "core/movement.h"

#include <cmath>

namespace
{
constexpr float kStep = 18.0f;          // stock's STEPSIZE
constexpr float kStepBelowZero = 20.0f; // MuffMode's STEPSIZE_BELOW
constexpr float kJump = 270.0f;
constexpr float kJumpBelowZero = 4.0f;
} // namespace

float Core_Movement_StepSize(bool classic, float z)
{
	return classic && z < 0 ? kStepBelowZero : kStep;
}

float Core_Movement_JumpSpeed(bool classic, float vz, float z)
{
	if (!classic)
		return vz + kJump < kJump ? kJump : vz + kJump; // stock's
	const float jump = z < 0 ? kJump + kJumpBelowZero : kJump;
	const float speed = std::ceil(vz + jump);
	return speed < jump ? jump : speed;
}
