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

# Writes ${PROJECT_BINARY_DIR}/public/manifest.txt, one line per target that
# stages headers: <target>|<staged root>|<linked targets that stage headers>|<headers>
# The guards (cmake/check_public_headers.cmake, public_headers_compile) read it.
function(ursatz_write_public_manifest)
    set(lines "")
    set(targets "")
    set(pending ${PROJECT_SOURCE_DIR})
    while(pending)
        list(POP_FRONT pending dir)
        get_property(dir_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        list(APPEND targets ${dir_targets})
        get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        list(APPEND pending ${subdirs})
    endwhile()
    list(REMOVE_DUPLICATES targets)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "STATIC_LIBRARY")
            continue()
        endif()
        get_target_property(root ${target} URSATZ_PUBLIC_ROOT)
        if(NOT root)
            continue()
        endif()
        get_target_property(headers ${target} URSATZ_PUBLIC_HEADERS)
        get_target_property(libs ${target} LINK_LIBRARIES)
        set(linked "")
        foreach(lib IN LISTS libs)
            if(TARGET ${lib})
                get_target_property(lib_root ${lib} URSATZ_PUBLIC_ROOT)
                if(lib_root)
                    list(APPEND linked ${lib})
                endif()
            endif()
        endforeach()
        string(REPLACE ";" "," headers_csv "${headers}")
        string(REPLACE ";" "," linked_csv "${linked}")
        list(APPEND lines "${target}|${root}|${linked_csv}|${headers_csv}")
    endforeach()
    list(JOIN lines "\n" text)
    file(WRITE "${PROJECT_BINARY_DIR}/public/manifest.txt" "${text}\n")
endfunction()

# The guards of the public header sets (R-223, R-248):
#   architecture_public_headers  a public header includes only public headers
#   public_headers_compile       every public header of a target compiles alone,
#                                with only the staged roots of the target and its links
function(ursatz_add_public_header_tests)
    add_test(NAME architecture_public_headers
             COMMAND ${CMAKE_COMMAND} -DBINARY_DIR=${PROJECT_BINARY_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/check_public_headers.cmake)

    file(STRINGS "${PROJECT_BINARY_DIR}/public/manifest.txt" lines)
    set(objects "")
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^([^|]*)\\|([^|]*)\\|([^|]*)\\|(.*)$")
            continue()
        endif()
        set(name "${CMAKE_MATCH_1}")
        set(linked "${CMAKE_MATCH_3}")
        string(REPLACE "," ";" headers "${CMAKE_MATCH_4}")
        if(NOT headers)
            continue()
        endif()
        # the roots of the target and of everything it links, transitively
        set(todo ${name})
        set(seen "")
        set(roots "")
        while(todo)
            list(POP_FRONT todo t)
            if(t IN_LIST seen)
                continue()
            endif()
            list(APPEND seen ${t})
            get_target_property(root ${t} URSATZ_PUBLIC_ROOT)
            if(root)
                list(APPEND roots "${root}")
            endif()
            get_target_property(libs ${t} LINK_LIBRARIES)
            foreach(lib IN LISTS libs)
                if(TARGET ${lib})
                    list(APPEND todo ${lib})
                endif()
            endforeach()
        endwhile()
        set(source "${PROJECT_BINARY_DIR}/public/check_${name}.c")
        set(text "/* generated: every public header of ${name} */\n")
        foreach(header IN LISTS headers)
            string(APPEND text "#include \"${header}\"\n")
        endforeach()
        file(GENERATE OUTPUT "${source}" CONTENT "${text}")
        add_library(public_headers_compile_${name} OBJECT EXCLUDE_FROM_ALL "${source}")
        set_source_files_properties("${source}" PROPERTIES GENERATED TRUE)
        # only the staged roots (no source directory); system headers (sqlite3.h) come with the compiler
        target_include_directories(public_headers_compile_${name} PRIVATE ${roots})
        list(APPEND objects public_headers_compile_${name})
    endforeach()
    add_custom_target(public_headers_compile_all DEPENDS ${objects})
    add_test(NAME public_headers_compile
             COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target public_headers_compile_all)
endfunction()
