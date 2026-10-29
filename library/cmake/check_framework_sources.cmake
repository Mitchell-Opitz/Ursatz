# Fails when a framework or the chain CMakeLists names a source under src/
# (R-241: frameworks never compile kernel sources). Run with
#   cmake -DSOURCE_DIR=<repo root> -P cmake/check_framework_sources.cmake
if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

file(GLOB framework_lists "${SOURCE_DIR}/frameworks/*/CMakeLists.txt")
set(lists ${framework_lists} "${SOURCE_DIR}/chain/CMakeLists.txt")

set(violations 0)
foreach(list_file IN LISTS lists)
    if(NOT EXISTS "${list_file}")
        continue()
    endif()
    file(STRINGS "${list_file}" lines)
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "#.*$" "" code "${line}")
        if(code MATCHES "PROJECT_SOURCE_DIR}/src/" OR code MATCHES "CMAKE_SOURCE_DIR}/src/" OR code MATCHES "\\.\\./src/")
            message(SEND_ERROR "${list_file} names a kernel source: ${line}")
            math(EXPR violations "${violations} + 1")
        endif()
    endforeach()
endforeach()

if(violations GREATER 0)
    message(FATAL_ERROR "${violations} framework or chain source(s) under src/")
endif()
message(STATUS "framework sources: none under src/")
