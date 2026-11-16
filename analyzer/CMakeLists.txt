cmake_minimum_required(VERSION 3.20)
project(UrsatzAnalyzer VERSION 0.7.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(src)
if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
