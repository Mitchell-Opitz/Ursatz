# Public headers (R-223): a library target exposes only the headers it names,
# staged at configure time, and keeps its source directories to itself.
#
#   ursatz_public_headers(<target>
#       BASE <dir>                 directory the HEADERS are relative to
#       [PREFIX <prefix>]          folder the headers are staged under (none: the BASE layout)
#       [SOURCE_DIRS <dir>...]     the target's source directories (PRIVATE)
#       [HEADERS <relative path>...])   none: the target has no public header yet
#
# Each header is copied to ${PROJECT_BINARY_DIR}/public/<target>/<prefix>/<path>
# and ${PROJECT_BINARY_DIR}/public/<target> becomes a PUBLIC include directory.
# With URSATZ_LEGACY_INCLUDE_DIRS the source directories are also PUBLIC, as before
# the public sets (an app turns it on at its bump and off once its includes are
# switched, P24, P25, P27).
# Every source header is a configure dependency, so an edited header is staged
# again by the next build. The target's properties record what was staged:
#   URSATZ_PUBLIC_ROOT     the staged include root
#   URSATZ_PUBLIC_HEADERS  the staged headers, as included (<prefix>/<path>)
#   URSATZ_SOURCE_DIRS     the source directories
function(ursatz_public_headers target)
    cmake_parse_arguments(ARG "" "BASE;PREFIX" "HEADERS;SOURCE_DIRS" ${ARGN})
    if(NOT ARG_BASE)
        message(FATAL_ERROR "ursatz_public_headers(${target}): BASE is required")
    endif()

    set(root "${PROJECT_BINARY_DIR}/public/${target}")
    set(staged "")
    file(MAKE_DIRECTORY "${root}")
    foreach(header IN LISTS ARG_HEADERS)
        if(ARG_PREFIX)
            set(as_included "${ARG_PREFIX}/${header}")
        else()
            set(as_included "${header}")
        endif()
        if(NOT EXISTS "${ARG_BASE}/${header}")
            message(FATAL_ERROR "ursatz_public_headers(${target}): no such header ${ARG_BASE}/${header}")
        endif()
        configure_file("${ARG_BASE}/${header}" "${root}/${as_included}" COPYONLY)
        list(APPEND staged "${as_included}")
    endforeach()

    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${root}>)
    if(ARG_SOURCE_DIRS)
        target_include_directories(${target} PRIVATE ${ARG_SOURCE_DIRS})
        if(URSATZ_LEGACY_INCLUDE_DIRS)
            foreach(dir IN LISTS ARG_SOURCE_DIRS)
                target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${dir}>)
            endforeach()
        endif()
    endif()
    set_target_properties(${target} PROPERTIES
        URSATZ_PUBLIC_ROOT "${root}"
        URSATZ_PUBLIC_HEADERS "${staged}"
        URSATZ_SOURCE_DIRS "${ARG_SOURCE_DIRS}")
endfunction()

# A test sees the internals of the code it tests: the kernel sources, the
# framework folders, the chain, and the folder it sits beside. An app does not.
# Call it first in a tests/CMakeLists.txt.
macro(ursatz_test_includes)
    include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/frameworks
        ${PROJECT_SOURCE_DIR}/chain
        ${CMAKE_CURRENT_SOURCE_DIR}/..)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../analysis")
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../analysis)
    endif()
endmacro()
