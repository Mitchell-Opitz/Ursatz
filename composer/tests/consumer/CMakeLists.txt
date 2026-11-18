cmake_minimum_required(VERSION 3.20)
project(UrsatzComposerConsumer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# An application that vendors the repository's third_party, the library and the Analyzer adds them first, then the
# Composer, which reuses their targets and builds neither its command nor its tests (the way the GUI links it).
set(URSATZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH "The repository root")
set(URSATZ_GUI OFF)
set(URSATZ_TESTS OFF)
set(URSATZ_COMPOSER_BUILD_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(${URSATZ_ROOT}/third_party third_party)
add_subdirectory(${URSATZ_ROOT}/library library)
add_subdirectory(${URSATZ_ROOT}/analyzer analyzer)
add_subdirectory(${URSATZ_ROOT}/composer composer)

add_executable(consumer consumer.c)
# The consumer builds the knowledge snapshot the call context holds, so it links the library's theory itself.
target_link_libraries(consumer PRIVATE ursatz_composer ursatz_theory cjson)
target_compile_options(consumer PRIVATE -Wall -Wextra -Wpedantic)

# Each of these must fail to compile, because the Composer links its libraries PRIVATE and installs one public header.
# They are not built by default; consumer_rejects_internals builds them one at a time and checks the reason.
foreach(reject reject_composer_header reject_analyzer_header reject_library_header)
    add_executable(${reject} EXCLUDE_FROM_ALL ${reject}.c)
    target_link_libraries(${reject} PRIVATE ursatz_composer)
endforeach()
