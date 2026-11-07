# The mod's identity comes from mod.json and nowhere else (AGENTS.md; checked by
# scripts/check_name_leak.py). A downstream mod points MOD_IDENTITY_FILE at its
# own mod.json to build the same core under its own name.
#
#   name     display name: banner, g_version
#   id       short lowercase identifier: serverinfo gamename
#   version  release version, MAJOR.MINOR.PATCH
#   repository  (optional) the project's page: the Tab menu's About
#
# The build id is the git commit (with -dirty for uncommitted changes), or
# "nogit" when built from a source archive without git.

set(MOD_IDENTITY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" CACHE FILEPATH "The mod's identity file")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MOD_IDENTITY_FILE}")

file(READ "${MOD_IDENTITY_FILE}" _mod_json)
foreach(_key name id version)
    string(JSON _value ERROR_VARIABLE _err GET "${_mod_json}" "${_key}")
    if(_err OR _value STREQUAL "")
        message(FATAL_ERROR "${MOD_IDENTITY_FILE}: missing \"${_key}\"")
    endif()
    string(TOUPPER "${_key}" _upper)
    set(MOD_${_upper} "${_value}")
endforeach()

if(NOT MOD_ID MATCHES "^[a-z][a-z0-9_]*$")
    message(FATAL_ERROR "mod.json: id must be lowercase letters, digits and _ (got \"${MOD_ID}\")")
endif()
if(NOT MOD_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "mod.json: version must be MAJOR.MINOR.PATCH (got \"${MOD_VERSION}\")")
endif()
# Optional: shown in the Tab menu's About page.
string(JSON MOD_REPOSITORY ERROR_VARIABLE _err GET "${_mod_json}" "repository")
if(_err)
    set(MOD_REPOSITORY "")
endif()
if(MOD_REPOSITORY MATCHES "[\"\\\\]")
    message(FATAL_ERROR "mod.json: repository must not contain quotes or backslashes")
endif()
if(MOD_NAME MATCHES "[\"\\\\]")
    message(FATAL_ERROR "mod.json: name must not contain quotes or backslashes")
endif()

# The commit of the repository that holds the identity file: this base's, or
# a downstream mod's (with this base as its submodule).
find_package(Git QUIET)
set(MOD_BUILD "nogit")
get_filename_component(_mod_dir "${MOD_IDENTITY_FILE}" DIRECTORY)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=8 --exclude "*"
        WORKING_DIRECTORY "${_mod_dir}" OUTPUT_VARIABLE _build
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND _build)
        set(MOD_BUILD "${_build}")
    endif()
endif()

set(MOD_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/mod_identity.h.in" "${MOD_GENERATED_DIR}/mod_identity.h" @ONLY)
message(STATUS "Mod identity: ${MOD_NAME} (${MOD_ID}) ${MOD_VERSION}, build ${MOD_BUILD}")
