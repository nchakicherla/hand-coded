cmake_minimum_required(VERSION 3.10)
project(hand-coded LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra)

set(SOURCES src/main.c src/arena.c src/file.c)

add_library(csv STATIC external/libcsv/libcsv.c)
target_include_directories(csv PUBLIC external/libcsv)

add_executable(main ${SOURCES})
target_link_libraries(main PRIVATE csv)
