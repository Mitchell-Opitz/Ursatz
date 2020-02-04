cmake_minimum_required(VERSION 3.20)
project(UrsatzAnalyzer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(external/ursatz)
add_subdirectory(external/cjson)
add_subdirectory(src)
add_subdirectory(tests)
