cmake_minimum_required(VERSION 3.10)
project(hand-coded LANGUAGES C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra)

set(SOURCES src/main.c)

add_executable(main ${SOURCES})
