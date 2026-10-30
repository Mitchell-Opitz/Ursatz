cmake_minimum_required(VERSION 3.20)
project(UrsatzAnalyzer VERSION 0.3.0 LANGUAGES C)

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

# Ursatz submodule commit, recorded in every analysis document's metadata.
set(URSATZ_ANALYZER_URSATZ_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/external/ursatz
        RESULT_VARIABLE ursatz_commit_result
        OUTPUT_VARIABLE ursatz_commit_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(ursatz_commit_result EQUAL 0 AND ursatz_commit_output MATCHES "^[0-9a-f]+$")
        set(URSATZ_ANALYZER_URSATZ_COMMIT "${ursatz_commit_output}")
    endif()
endif()

# A parent project that already provides Ursatz supplies the ursatz target itself.
if(NOT TARGET ursatz)
    add_subdirectory(external/ursatz)
endif()
# A parent project that vendors cJSON supplies the cjson target itself.
if(NOT TARGET cjson)
    add_subdirectory(external/cjson)
endif()
add_subdirectory(src)
if(URSATZ_ANALYZER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
