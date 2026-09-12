cmake_minimum_required(VERSION 3.24)

project(Lufia1Recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Repository root:
#
#   Lufia-Estpolis-Recompendium/
#     games/
#       lufia1/
#
get_filename_component(
    RECOMPENDIUM_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE)

#
# Generated SNESRecomp output is intentionally local and uncommitted.
#
# The default currently points at the known-good Milestone 1 baseline.
# It may be overridden at configure time with:
#
#   -DLUFIA1_GENERATED_ROOT=/path/to/generated/output
#
set(
    LUFIA1_GENERATED_ROOT
    "${RECOMPENDIUM_ROOT}/local/generated/lufia1-test2"
    CACHE PATH
    "Path to generated SNESRecomp output for Lufia I")

set(
    SNESRECOMP_ROOT
    "${LUFIA1_GENERATED_ROOT}/snesrecomp")

if(NOT EXISTS "${SNESRECOMP_ROOT}/runner/runner.cmake")
    message(FATAL_ERROR
        "Could not find the generated SNESRecomp runner at:\n"
        "  ${SNESRECOMP_ROOT}/runner/runner.cmake\n"
        "Generate Lufia I with SNESRecomp first, or set "
        "LUFIA1_GENERATED_ROOT explicitly.")
endif()

if(NOT EXISTS
   "${LUFIA1_GENERATED_ROOT}/build/libsnesrecomp_game.a")
    message(FATAL_ERROR
        "Could not find libsnesrecomp_game.a at:\n"
        "  ${LUFIA1_GENERATED_ROOT}/build/libsnesrecomp_game.a\n"
        "Build the generated SNESRecomp project first.")
endif()

#
# Keep the initial Lufia I runtime deliberately close to the stock
# SNESRecomp hardware model.
#
set(SNESRECOMP_ENABLE_TRACE OFF CACHE BOOL "" FORCE)
set(SNES_COSIM OFF CACHE BOOL "" FORCE)
set(SNESRECOMP_ENABLE_MODS OFF CACHE BOOL "" FORCE)

include("${SNESRECOMP_ROOT}/runner/runner.cmake")

#
# The Recompendium host supplies its own launcher and input handling.
#
list(
    FILTER SNESRECOMP_RUNNER_SOURCES
    EXCLUDE REGEX "launcher(_cache|_picker)?\\.c$")

list(
    FILTER SNESRECOMP_RUNNER_SOURCES
    EXCLUDE REGEX "keybinds\\.c$")

#
# ROM-derived generated code remains outside Git and is linked as an
# imported static library.
#
add_library(
    lufia1_generated
    STATIC
    IMPORTED
    GLOBAL)

set_target_properties(
    lufia1_generated
    PROPERTIES
    IMPORTED_LOCATION
    "${LUFIA1_GENERATED_ROOT}/build/libsnesrecomp_game.a")

#
# Permanent Lufia I host/runtime.
#
add_executable(
    lufia1

    ${SNESRECOMP_RUNNER_SOURCES}

    src/main.c
    src/host_shims.c
    src/lufia1_game_info.c
    src/lufia1_runtime.c
)

snesrecomp_target_sdl(lufia1)

target_include_directories(
    lufia1
    PRIVATE

    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${LUFIA1_GENERATED_ROOT}/generated"

    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}

    "${SNESRECOMP_ROOT}/runner/src/desktop"
)

target_compile_definitions(
    lufia1
    PRIVATE

    SNESRECOMP_TRACE=0
    SNESRECOMP_REVERSE_DEBUG=0
    SNESRECOMP_LLE_BOUNCE_DEFAULT=1
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
)

target_link_libraries(
    lufia1
    PRIVATE

    lufia1_generated
    ${SNESRECOMP_RUNNER_LIBRARIES}
)

#
# The generated runner currently contains code paths whose warnings are
# outside the scope of the Lufia I host. Keep the baseline build quiet
# until those dependencies are separated more cleanly.
#
if(NOT MSVC)
    target_compile_options(
        lufia1
        PRIVATE
        -w
        -Wno-implicit-function-declaration)
endif()
