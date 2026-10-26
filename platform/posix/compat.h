/*
 * compat.h - force-included into the upstream game sources.
 *
 * Carries the handful of declarations MSVC provides implicitly and libstdc++
 * does not. Applied with -include so id's files need no edit to build on
 * POSIX. Adapted from Q2RED's game-rerelease/compat/q2red_upstream_compat.h.
 *
 * Keep it to declarations. If something here starts changing behaviour rather
 * than availability, it belongs in a documented patch instead, with a reason.
 */

#pragma once

#include <cmath>

/* g_phys.cpp and g_save.cpp call isnan() unqualified. <math.h> makes that work
 * on MSVC; in C++ libstdc++ puts it in namespace std and defines no macro. */
using std::isnan;
using std::isinf;
