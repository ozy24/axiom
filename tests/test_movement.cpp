// Classic jumps (core/movement.h, decision 0024).
#include "core/movement.h"

#include "doctest.h"

TEST_CASE("classic jumps: off is stock everywhere")
{
	CHECK(Core_Movement_StepSize(false, -100) == 18.0f);
	CHECK(Core_Movement_StepSize(false, 100) == 18.0f);
	CHECK(Core_Movement_JumpSpeed(false, 0, -100) == 270.0f);
	CHECK(Core_Movement_JumpSpeed(false, 10.5f, 100) == 280.5f);
	CHECK(Core_Movement_JumpSpeed(false, -50, 100) == 270.0f); // never less than a jump
}

TEST_CASE("classic jumps: higher steps and jumps below height 0 only")
{
	CHECK(Core_Movement_StepSize(true, -0.125f) == 20.0f);
	CHECK(Core_Movement_StepSize(true, 0) == 18.0f);
	CHECK(Core_Movement_StepSize(true, 64) == 18.0f);
	CHECK(Core_Movement_JumpSpeed(true, 0, -200) == 274.0f);
	CHECK(Core_Movement_JumpSpeed(true, 0, 200) == 270.0f);
	CHECK(Core_Movement_JumpSpeed(true, -50, -200) == 274.0f);
}

TEST_CASE("classic jumps: a jump's speed rounds up, as MuffMode's")
{
	CHECK(Core_Movement_JumpSpeed(true, 10.25f, 200) == 281.0f);
	CHECK(Core_Movement_JumpSpeed(true, 10.25f, -200) == 285.0f);
}

TEST_CASE("teleporters without the freeze: out of the exit at the speed in")
{
	// In at 320 heading north, out of an exit facing east (yaw 0).
	MoveVec v = Core_Movement_TeleportVelocity({0, 320, 0}, {0, 0, 0});
	CHECK(v[0] == doctest::Approx(320));
	CHECK(v[1] == doctest::Approx(0).epsilon(0.001));
	CHECK(v[2] == doctest::Approx(0).epsilon(0.001));
	// Falling in counts too (MuffMode's: the whole speed), out facing west.
	v = Core_Movement_TeleportVelocity({300, 0, -400}, {0, 180, 0});
	CHECK(v[0] == doctest::Approx(-500));
	CHECK(v[1] == doctest::Approx(0).epsilon(0.001));
	// Standing still in: standing still out.
	v = Core_Movement_TeleportVelocity({0, 0, 0}, {0, 90, 0});
	CHECK(v[0] == 0);
	CHECK(v[1] == 0);
}

TEST_CASE("water-jump probe: stock's steps for positive gravity, bounded otherwise")
{
	// stock: min(50, int(10 * 800 / gravity))
	CHECK(Core_Movement_WaterJumpSteps(800.0f) == 10);
	CHECK(Core_Movement_WaterJumpSteps(1600.0f) == 5);
	CHECK(Core_Movement_WaterJumpSteps(100.0f) == 50);
	CHECK(Core_Movement_WaterJumpSteps(300.0f) == 26);
	// stock wraps: a negative count compared with size_t is billions of traces
	CHECK(Core_Movement_WaterJumpSteps(-800.0f) == 10);
	CHECK(Core_Movement_WaterJumpSteps(-1.0f) == 50);
	// stock casts infinity to int (undefined)
	CHECK(Core_Movement_WaterJumpSteps(0.0f) == 50);
}
