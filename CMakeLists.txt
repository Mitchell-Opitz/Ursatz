cmake_minimum_required(VERSION 3.28)
project(Ursatz LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Debug unless asked otherwise: Release compiles asserts out and the tests rely on them.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(URSATZ_GUI "Build the GUI and its HTTP server" ON)
option(URSATZ_TESTS "Build and register the tests of every part" ON)

# Each part reads its own switch; the root sets them all from the two options above.
set(URSATZ_BUILD_TESTS ${URSATZ_TESTS})
set(URSATZ_ANALYZER_BUILD_TESTS ${URSATZ_TESTS})
set(URSATZ_COMPOSER_BUILD_TESTS ${URSATZ_TESTS})
set(URSATZ_COMPOSER_BUILD_CLI ON)

enable_testing()

# Dependencies first; each later part reuses the targets the earlier ones define.
add_subdirectory(third_party)
add_subdirectory(library)
add_subdirectory(analyzer)
add_subdirectory(composer)
if(URSATZ_GUI)
    add_subdirectory(gui)
endif()
