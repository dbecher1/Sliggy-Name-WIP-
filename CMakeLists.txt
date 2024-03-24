cmake_minimum_required(VERSION 3.28)
project(slippry C)

set(CMAKE_C_STANDARD 11)
find_package(SDL2 REQUIRED)

if (SDL2_FOUND)
    message("SDL2 version: ${SDL2_VERSION}")
    message("SDL2 include directory found: ${SDL2_INCLUDE_DIR}")
    message("SDL2 library found: ${SDL2_LIBRARIES}")
endif ()

include_directories(${SDL2_INCLUDE_DIR})

add_executable(slippry main.c
        Slippry.h
        Slippry.c)
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})