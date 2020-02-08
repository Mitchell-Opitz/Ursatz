cmake_minimum_required(VERSION 3.20)
project(UrsatzAnalyzer VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

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

add_subdirectory(external/ursatz)
add_subdirectory(external/cjson)
add_subdirectory(src)
add_subdirectory(tests)
