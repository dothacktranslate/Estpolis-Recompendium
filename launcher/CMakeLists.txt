cmake_minimum_required(VERSION 3.20)
project(estopolis_launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)

add_executable(estopolis-launcher
    src/main.cpp
)

target_link_libraries(estopolis-launcher PRIVATE
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    PkgConfig::SDL2_TTF
)
