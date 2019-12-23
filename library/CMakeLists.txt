cmake_minimum_required(VERSION 3.20)
project(Ursatz C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(src)
add_subdirectory(frameworks)