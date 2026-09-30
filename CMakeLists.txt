cmake_minimum_required(VERSION 3.20)
project(wolfie3d C)
set(CMAKE_C_STANDARD 23)

add_subdirectory(external/glad)
add_subdirectory(external/glfw)

add_executable(wolfie3d src/main.c)
target_compile_options(wolfie3d PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(wolfie3d PRIVATE glfw glad)