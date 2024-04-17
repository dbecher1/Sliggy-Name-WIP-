cmake_minimum_required(VERSION 3.27)
project(Sliggy C)

set(CMAKE_C_STANDARD 11)
find_package(SDL2 REQUIRED)
find_package(SDL_image )

# Absolute path for my machine because my SDL image install is hecked up
set(SDL_IMAGE_INCLUDE_DIRS /usr/local/Cellar/sdl2_image/2.8.2_1/include/)
set (SDL_IMAGE_LIBRARIES /usr/local/Cellar/sdl2_image/2.8.2_1/lib/libSDL2_image.dylib)

include_directories(${SDL2_INCLUDE_DIR} ${SDL_IMAGE_INCLUDE_DIRS})

add_executable(Sliggy main.c
        Sliggy.h
        Sliggy.c
)
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL_IMAGE_LIBRARIES})