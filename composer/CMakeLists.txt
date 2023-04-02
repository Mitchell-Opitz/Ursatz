cmake_minimum_required(VERSION 3.20)
project(UrsatzComposer VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(URSATZ_COMPOSER_TESTS_DEFAULT ON)
else()
    set(URSATZ_COMPOSER_TESTS_DEFAULT OFF)
endif()
option(URSATZ_COMPOSER_BUILD_TESTS "Build and register composer tests" ${URSATZ_COMPOSER_TESTS_DEFAULT})

if(URSATZ_COMPOSER_BUILD_TESTS)
    enable_testing()
endif()

# Subprojects are built without their tests; Ursatz must come first so the Analyzer reuses its target.
set(URSATZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(URSATZ_ANALYZER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/ursatz)
add_subdirectory(external/ursatz-analyzer)
add_subdirectory(src)
add_subdirectory(cli)
if(URSATZ_COMPOSER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
