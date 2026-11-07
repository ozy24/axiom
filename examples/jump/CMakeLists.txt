# A downstream mod built on this base without changing it (docs/downstream.md):
# its own identity (mod.json) and its own gametype (gametypes/jump), with the
# base's four alongside. In a real mod the base is a git submodule; here it is
# this repository, two levels up, so CI builds the example on every PR.
#
#   cmake -S examples/jump -B build/jump -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build/jump      # build/jump/base/game_x64.so (or .dll)
cmake_minimum_required(VERSION 3.21)
project(jump_example LANGUAGES CXX)

# Set before the base is added: who the module says it is, where the mod's
# gametypes live, and which gametypes it registers (ffa must stay).
set(MOD_IDENTITY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mod.json")
set(GAME_GAMETYPE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/gametypes")
set(GAME_GAMETYPES ffa duel tdm ctf jump)
set(GAME_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
set(GAME_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "The base mod's source (a submodule in a real mod)")
add_subdirectory("${GAME_BASE_DIR}" base)

# The mod's own unit tests: its pure logic, on the base's doctest.
option(JUMP_BUILD_TESTS "Build the mod's unit tests" ON)
if(JUMP_BUILD_TESTS)
    enable_testing()
    add_executable(jump_tests tests/test_jump.cpp gametypes/jump/jump.cpp)
    target_include_directories(jump_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${GAME_BASE_DIR}/third_party/doctest-2.4.11")
    set_target_properties(jump_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_test(NAME jump_tests COMMAND jump_tests)
endif()
