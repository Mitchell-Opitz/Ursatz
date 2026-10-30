cmake_minimum_required(VERSION 3.20)

# Fails when a framework or chain source includes a header of another framework
# by its bare name (R-223, R-248): "framework.h" can mean any of the frameworks,
# so a header of another framework is included by its public path
# ("standard-form/analysis/analysis_phrase.h"). A sibling or a header of the
# same framework stays bare. Run with
#   cmake -DSOURCE_DIR=<repo root> -P cmake/check_header_paths.cmake
if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

# Every header of the frameworks and the chain, by the area it belongs to.
file(GLOB framework_dirs LIST_DIRECTORIES true "${SOURCE_DIR}/frameworks/*")
set(areas "")
foreach(dir IN LISTS framework_dirs)
    if(IS_DIRECTORY "${dir}")
        get_filename_component(area "${dir}" NAME)
        list(APPEND areas "${area}")
        set(root_${area} "${dir}")
    endif()
endforeach()
list(APPEND areas chain)
set(root_chain "${SOURCE_DIR}/chain")

foreach(area IN LISTS areas)
    file(GLOB_RECURSE found "${root_${area}}/*.h")
    set(names_${area} "")
    foreach(path IN LISTS found)
        get_filename_component(file_name "${path}" NAME)
        list(APPEND names_${area} "${file_name}")
    endforeach()
endforeach()

set(violations 0)
foreach(area IN LISTS areas)
    file(GLOB_RECURSE sources "${root_${area}}/*.c" "${root_${area}}/*.h")
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<][^/\">]+[\">]")
        foreach(include_line IN LISTS includes)
            string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[\"<]([^\">]+)[\">].*" "\\1" included "${include_line}")
            if(included IN_LIST names_${area})
                continue() # a header of its own framework (a sibling or from its include directories)
            endif()
            foreach(other IN LISTS areas)
                if(NOT other STREQUAL area AND included IN_LIST names_${other})
                    file(RELATIVE_PATH shown "${SOURCE_DIR}" "${source}")
                    message(SEND_ERROR "${shown} includes \"${included}\", a header of ${other}, by its bare name")
                    math(EXPR violations "${violations} + 1")
                endif()
            endforeach()
        endforeach()
    endforeach()
endforeach()

if(violations GREATER 0)
    message(FATAL_ERROR "${violations} bare include(s) of another framework's header")
endif()
message(STATUS "header paths: no bare include of another framework's header")
