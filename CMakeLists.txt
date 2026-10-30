cmake_minimum_required(VERSION 3.21)

# The project name is deliberately generic: the mod's identity comes from
# mod.json (Phase 1c), never from build-system names.
project(q2game LANGUAGES CXX)

set(GAME_SOURCE_DIR "${CMAKE_SOURCE_DIR}/rerelease")

include(cmake/identity.cmake)

# The engine loads the client game (GetCGameAPI) from the same module as the
# server game. Retail clients need it, so Windows builds always include it.
# Linux builds feed the Q2RED dedicated server, which never loads it; it is off
# there until cgame code first changes.
if(WIN32)
    set(GAME_BUILD_CGAME_DEFAULT ON)
else()
    set(GAME_BUILD_CGAME_DEFAULT OFF)
endif()
option(GAME_BUILD_CGAME "Compile the client game (cg_*.cpp) into the module"
    ${GAME_BUILD_CGAME_DEFAULT})

# id's game.vcxproj compiles every .cpp under rerelease/ (checked: the
# project's ClCompile list and the files on disk are the same 92 files).
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_SOURCE_DIR}/*.cpp")
if(NOT GAME_BUILD_CGAME)
    list(FILTER GAME_SOURCES EXCLUDE REGEX "/cg_[^/]*\\.cpp$")
endif()

# --- Third-party -------------------------------------------------------------

# fmt 10.2.1 is vendored because id's fmt::formatter<edict_t> (g_local.h) has a
# non-const format(), which fmt 11+ rejects. q_std.cpp compiles fmt's
# src/format.cc itself unless NO_FMT_SOURCE is defined, so fmt needs only its
# include path here.
set(GAME_FMT_DIR "${CMAKE_SOURCE_DIR}/third_party/fmt-10.2.1")

# jsoncpp 1.9.6 is vendored and linked statically, so the module carries no
# runtime dependency on a distro's libjsoncpp.
set(GAME_JSONCPP_DIR "${CMAKE_SOURCE_DIR}/third_party/jsoncpp-1.9.6")
add_library(game_jsoncpp STATIC
    "${GAME_JSONCPP_DIR}/src/lib_json/json_reader.cpp"
    "${GAME_JSONCPP_DIR}/src/lib_json/json_value.cpp"
    "${GAME_JSONCPP_DIR}/src/lib_json/json_writer.cpp")
target_include_directories(game_jsoncpp
    PUBLIC "${GAME_JSONCPP_DIR}/include"
    PRIVATE "${GAME_JSONCPP_DIR}/src/lib_json")
set_target_properties(game_jsoncpp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# --- Gametypes ---------------------------------------------------------------

# The enabled gametypes, each in gametypes/<id>/gametype.cpp defining
# `gametype_<id>` (docs/design/gametype-api.md). A downstream mod sets this to
# its own list; ffa must stay in it.
# A plain default rather than a cache entry, so a change to the list here
# reaches existing build trees; a downstream mod passes -DGAME_GAMETYPES=...
if(NOT DEFINED GAME_GAMETYPES)
    set(GAME_GAMETYPES "ffa;duel;tdm;ctf")
endif()
if(NOT "ffa" IN_LIST GAME_GAMETYPES)
    message(FATAL_ERROR "GAME_GAMETYPES must include ffa (the fallback)")
endif()
set(GAMETYPE_DECLS "")
set(GAMETYPE_ENTRIES "")
set(GAME_GAMETYPE_SOURCES "")
foreach(gt IN LISTS GAME_GAMETYPES)
    if(NOT gt MATCHES "^[a-z][a-z0-9_]*$" OR NOT EXISTS "${CMAKE_SOURCE_DIR}/gametypes/${gt}/gametype.cpp")
        message(FATAL_ERROR "gametype '${gt}': expected gametypes/${gt}/gametype.cpp")
    endif()
    string(APPEND GAMETYPE_DECLS "extern const GametypeDef gametype_${gt};\n")
    string(APPEND GAMETYPE_ENTRIES "&gametype_${gt}, ")
    # Every .cpp in the gametype's folder: its definition, logic and adapter.
    file(GLOB _gt_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/gametypes/${gt}/*.cpp")
    list(APPEND GAME_GAMETYPE_SOURCES ${_gt_sources})
endforeach()
string(REPLACE "\\n" "\n" GAMETYPE_DECLS "${GAMETYPE_DECLS}")
configure_file("${CMAKE_SOURCE_DIR}/cmake/gametype_registry.cpp.in"
    "${MOD_GENERATED_DIR}/gametype_registry.cpp" @ONLY)

# --- The game module ---------------------------------------------------------

add_library(game_x64 SHARED ${GAME_SOURCES}
    # Compile-time checks that the stat and ammo/powerup layout stock clients
    # depend on is unchanged (AGENTS.md hard rule 2).
    "${CMAKE_SOURCE_DIR}/checks/stock_client_contract.cpp"
    # The mod's own code (AGENTS.md: small focused files, not id's).
    "${CMAKE_SOURCE_DIR}/core/eyecam_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/gametype.cpp"
    "${CMAKE_SOURCE_DIR}/core/gametype_register.cpp"
    ${GAME_GAMETYPE_SOURCES}
    "${MOD_GENERATED_DIR}/gametype_registry.cpp"
    "${CMAKE_SOURCE_DIR}/core/arena.cpp"
    "${CMAKE_SOURCE_DIR}/core/arena_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/host_files.cpp"
    "${CMAKE_SOURCE_DIR}/core/host_files_gi.cpp"
    "${CMAKE_SOURCE_DIR}/core/host_gi.cpp"
    "${CMAKE_SOURCE_DIR}/core/identity.cpp"
    "${CMAKE_SOURCE_DIR}/core/id_view_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/identity_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/maps.cpp"
    "${CMAKE_SOURCE_DIR}/core/maps_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/match.cpp"
    "${CMAKE_SOURCE_DIR}/core/match_record.cpp"
    "${CMAKE_SOURCE_DIR}/core/match_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/menu.cpp"
    "${CMAKE_SOURCE_DIR}/core/miniscore.cpp"
    "${CMAKE_SOURCE_DIR}/core/miniscore_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/scoreboard.cpp"
    "${CMAKE_SOURCE_DIR}/core/scoreboard_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/warmup_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/menu_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/vote.cpp"
    "${CMAKE_SOURCE_DIR}/core/vote_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/player_name_gi.cpp"
    "${CMAKE_SOURCE_DIR}/core/rulesets.cpp"
    "${CMAKE_SOURCE_DIR}/core/rulesets_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/stat_slots.cpp"
    "${CMAKE_SOURCE_DIR}/core/stat_slots_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/teams.cpp"
    "${CMAKE_SOURCE_DIR}/core/teams_register.cpp"
    "${CMAKE_SOURCE_DIR}/core/join.cpp"
    "${CMAKE_SOURCE_DIR}/core/join_register.cpp")

set_target_properties(game_x64 PROPERTIES
    # id's project uses C++17 (game.vcxproj LanguageStandard stdcpp17).
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    # The engine asks for exactly game_x64.dll / game_x64.so: no "lib" prefix.
    PREFIX ""
    OUTPUT_NAME "game_x64")

target_include_directories(game_x64 PRIVATE
    "${GAME_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "${MOD_GENERATED_DIR}"
    "${GAME_FMT_DIR}/include")
target_link_libraries(game_x64 PRIVATE game_jsoncpp)

# KEX_Q2_GAME and KEX_Q2GAME_DYNAMIC match id's game.vcxproj and are both
# required: without KEX_Q2_GAME, unknown client commands are echoed as chat
# (g_cmds.cpp:1745) instead of being refused.
target_compile_definitions(game_x64 PRIVATE KEX_Q2_GAME KEX_Q2GAME_DYNAMIC)

if(MSVC)
    # As game.vcxproj, except NO_FMT_SOURCE: fmt is vendored, not from vcpkg.
    target_compile_definitions(game_x64 PRIVATE
        KEX_Q2GAME_EXPORTS _CRT_SECURE_NO_WARNINGS)
    target_compile_options(game_x64 PRIVATE /permissive- /W3)
    # /MP parallelises MSBuild; Ninja already runs compiles in parallel, and
    # sccache won't cache a /MP compile.
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        target_compile_options(game_x64 PRIVATE /MP)
    endif()
    # Static CRT, as game.vcxproj (MultiThreaded / MultiThreadedDebug).
    set_property(TARGET game_x64 game_jsoncpp PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_sources(game_x64 PRIVATE
        "${CMAKE_SOURCE_DIR}/platform/posix/game_exports_posix.cpp")
    set_target_properties(game_x64 PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    # id's code is written for MSVC; GCC's warnings on it are not ours to fix.
    # New code (Phase 2) gets its own targets with warnings as errors.
    target_compile_options(game_x64 PRIVATE -w
        -include "${CMAKE_SOURCE_DIR}/platform/posix/compat.h")
    # GCC 12 (Ubuntu 22.04) rejects id's compile-time format strings in
    # q_std.h; GCC 13+ accepts them. Empty FMT_CONSTEVAL makes the check
    # run-time instead, with identical output.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
            AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        target_compile_definitions(game_x64 PRIVATE "FMT_CONSTEVAL=")
    endif()
    target_link_options(game_x64 PRIVATE -Wl,--no-undefined
        "-Wl,--version-script=${CMAKE_SOURCE_DIR}/platform/posix/exports.map")
    set_property(TARGET game_x64 APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_SOURCE_DIR}/platform/posix/exports.map")
    # Release builds keep libstdc++ and libgcc out of the module's runtime
    # dependencies so it loads on any distro the server binary does. Off by
    # default because Fedora needs the libstdc++-static package for it.
    option(GAME_STATIC_RUNTIME "Link libstdc++ and libgcc statically" OFF)
    if(GAME_STATIC_RUNTIME)
        target_link_options(game_x64 PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endif()

# --- Unit tests --------------------------------------------------------------

# The mod's own logic, without the engine (AGENTS.md: the Host seam). Adapters
# (*_register.cpp, *_gi.cpp) are deliberately left out: they need gi.
option(GAME_BUILD_TESTS "Build the unit tests (game_tests)" ON)
if(GAME_BUILD_TESTS)
    enable_testing()
    add_executable(game_tests
        tests/test_main.cpp
        tests/test_identity.cpp
        tests/test_gametype.cpp
        tests/test_stat_slots.cpp
        tests/test_match.cpp
        tests/test_rulesets.cpp
        tests/test_match_record.cpp
        tests/test_menu.cpp
        tests/test_vote.cpp
        tests/test_maps.cpp
        tests/test_miniscore.cpp
        tests/test_teams.cpp
        tests/test_join.cpp
        tests/test_scoreboard.cpp
        tests/test_arena.cpp
        core/arena.cpp
        core/identity.cpp
        core/match_record.cpp
        core/host_files.cpp
        core/rulesets.cpp
        core/maps.cpp
        core/match.cpp
        core/menu.cpp
        core/miniscore.cpp
        core/vote.cpp
        core/stat_slots.cpp
        core/teams.cpp
        core/join.cpp
        core/scoreboard.cpp
        tests/test_duel_queue.cpp
        tests/test_duel_board.cpp
        tests/test_eyecam.cpp
        core/gametype.cpp
        gametypes/ffa/gametype.cpp
        gametypes/duel/duel_queue.cpp
        gametypes/duel/duel_board.cpp)
    target_include_directories(game_tests PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/tests"
        "${MOD_GENERATED_DIR}"
        "${CMAKE_SOURCE_DIR}/third_party/doctest-2.4.11")
    set_target_properties(game_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(game_tests PRIVATE /W4 /WX /permissive-)
        set_property(TARGET game_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(game_tests PRIVATE -Wall -Wextra -Werror)
    endif()
    target_link_libraries(game_tests PRIVATE game_jsoncpp)
    add_test(NAME game_tests COMMAND game_tests)
endif()
