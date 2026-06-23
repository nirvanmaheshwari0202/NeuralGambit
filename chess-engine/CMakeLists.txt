cmake_minimum_required(VERSION 3.15)
project(liszt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(liszt engine.cpp)

# Optimize hard for the host CPU in Release builds.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(liszt PRIVATE /O2 /DNDEBUG)
    else()
        target_compile_options(liszt PRIVATE -O2 -march=native -DNDEBUG)
    endif()
endif()

# chess.hpp lives next to this file.
target_include_directories(liszt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
