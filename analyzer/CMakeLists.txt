cmake_minimum_required(VERSION 3.20)
project(UrsatzAnalyzer VERSION 0.7.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(URSATZ_ANALYZER_TESTS_DEFAULT ON)
else()
    set(URSATZ_ANALYZER_TESTS_DEFAULT OFF)
endif()
option(URSATZ_ANALYZER_BUILD_TESTS "Build and register analyzer tests" ${URSATZ_ANALYZER_TESTS_DEFAULT})

if(URSATZ_ANALYZER_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(src)
if(URSATZ_ANALYZER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
