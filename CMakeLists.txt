cmake_minimum_required(VERSION 3.20)
project(Ursatz LANGUAGES C)

# Debug unless asked otherwise: Release compiles asserts out and the tests rely on them.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

enable_testing()

option(URSATZ_BUILD_TESTS "Build and register the library's tests" ON)
option(URSATZ_ANALYZER_BUILD_TESTS "Build and register the Analyzer's tests" ON)
option(URSATZ_COMPOSER_BUILD_TESTS "Build and register the Composer's tests" ON)
option(URSATZ_COMPOSER_BUILD_CLI "Build the ursatz-composer command" ON)

# Dependencies first; each later part reuses the targets the earlier ones define.
add_subdirectory(library)
add_subdirectory(analyzer)
add_subdirectory(composer)
add_subdirectory(gui)
