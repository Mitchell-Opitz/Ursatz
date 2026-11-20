# Each application library exports only its API (R-640): the sources build as an engine archive, and the library the
# application's consumers link is that archive merged into one relocatable object whose only global symbols are the names of
# an export file (`ld -r` over the whole archive, then `objcopy --keep-global-symbols`). The tests of the internals link the
# engine, which keeps every symbol.
#
#   ursatz_localize_exports(<target> ENGINE <engine-target> KEEP <file>)
#       <file> holds one exported symbol per line. With URSATZ_LOCALIZE_EXPORTS OFF, <target> is an interface to the engine
#       archive itself. Never link <target> and the engine into one executable: they define the same API symbols.
#
# URSATZ_LOCALIZE_EXPORTS is ON by default where it can work: Linux with GNU ld and objcopy on the path. The reach scan
# (tools/ci/check-reach.sh) turns it OFF, as it reads the plain archives.

find_program(URSATZ_OBJCOPY objcopy)
find_program(URSATZ_NM nm)
find_program(URSATZ_LD ld)
set(_ursatz_localize_default OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND URSATZ_OBJCOPY AND URSATZ_LD)
    set(_ursatz_localize_default ON)
endif()
option(URSATZ_LOCALIZE_EXPORTS "Merge each application library into one object that exports only its API" ${_ursatz_localize_default})

function(ursatz_localize_exports target)
    cmake_parse_arguments(ARG "" "ENGINE;KEEP" "" ${ARGN})
    if(NOT ARG_ENGINE OR NOT ARG_KEEP OR ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ursatz_localize_exports(<target> ENGINE <target> KEEP <file>)")
    endif()
    if(NOT EXISTS "${ARG_KEEP}")
        message(FATAL_ERROR "ursatz_localize_exports(${target}): no such export file ${ARG_KEEP}")
    endif()
    if(NOT URSATZ_LOCALIZE_EXPORTS)
        add_library(${target} INTERFACE)
        target_link_libraries(${target} INTERFACE ${ARG_ENGINE})
        return()
    endif()

    set(merged "${CMAKE_CURRENT_BINARY_DIR}/${target}_merged.o")
    set(localized "${CMAKE_CURRENT_BINARY_DIR}/${target}_localized.o")
    add_custom_command(
        OUTPUT ${localized}
        COMMAND ${URSATZ_LD} -r -o ${merged} --whole-archive $<TARGET_FILE:${ARG_ENGINE}> --no-whole-archive
        COMMAND ${URSATZ_OBJCOPY} --keep-global-symbols=${ARG_KEEP} ${merged} ${localized}
        DEPENDS ${ARG_ENGINE} ${ARG_KEEP}
        COMMENT "Localizing the exports of ${target} to ${ARG_KEEP}"
        VERBATIM)
    add_library(${target} STATIC ${localized})
    set_source_files_properties(${localized} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE C)
    # What a consumer of the engine needs, a consumer of the localized object needs: its public headers and the libraries it
    # links (the engine's private ones for the link only). Both are read when this is called, so call it after the engine is set up.
    get_target_property(engine_includes ${ARG_ENGINE} INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(engine_links ${ARG_ENGINE} INTERFACE_LINK_LIBRARIES)
    if(engine_includes)
        target_include_directories(${target} PUBLIC ${engine_includes})
    endif()
    if(engine_links)
        target_link_libraries(${target} PUBLIC ${engine_links})
    endif()
endfunction()
