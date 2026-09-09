cmake_minimum_required(VERSION 3.24)

project(
    LufiaEstpolisRecompendium
    VERSION 0.1.0
    LANGUAGES C CXX
)

option(RECOMPENDIUM_BUILD_LUFIA1 "Build Lufia I" ON)
option(RECOMPENDIUM_BUILD_LUFIA2 "Build Lufia II" ON)

if(RECOMPENDIUM_BUILD_LUFIA1)
    add_subdirectory(games/lufia1)
endif()

if(RECOMPENDIUM_BUILD_LUFIA2)
    add_subdirectory(games/lufia2)
endif()
