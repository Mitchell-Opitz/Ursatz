# ursatz_test(): the one way to register a test (spec/target-architecture.md 12.2).
#
# ursatz_test(<name>
#     [SOURCES <file>...]            build an executable from these (default: <target>.c when no TARGET or COMMAND)
#     [TARGET <target>]              run this existing executable target instead of building one
#     [COMMAND <arg>...]             run this command instead of an executable (cmake -P scripts, node, a tool)
#     [ARGS <arg>...]                arguments after the executable
#     [LIBS <target>...]             linked PRIVATE to the built executable
#     [INCLUDES <dir>...]            PRIVATE include directories of the built executable
#     [DEFINITIONS <definition>...]  PRIVATE compile definitions of the built executable
#     [LABELS <label>...]            slow (nightly only), schema, accuracy, golden; valgrind comes with VALGRIND
#                                    (a COMMAND test that runs valgrind itself sets slow and valgrind)
#     [TIMEOUT <seconds>]            default 120, or 1800 for a slow test
#     [WORKING_DIRECTORY <dir>]
#     [ENVIRONMENT <var=value>...]
#     [VALGRIND]                     also register <name>_valgrind (or VALGRIND_NAME) under memcheck, labels slow and
#     [VALGRIND_NAME <name>]         valgrind, when valgrind is found
#     [PROPERTIES <property> <value>...])  any other test property (RESOURCE_LOCK, FIXTURES_*, DEPENDS...), on the
#                                    valgrind test too
#
# The executable target is <name> when <name> starts with test_, else test_<name>. Every built test executable is
# compiled with -UNDEBUG, so asserts hold in any build type, and with URSATZ_FIXTURES_DIR, the root tests/fixtures.
# Callers add their tests directory only when URSATZ_TESTS is on.
#
# ursatz_test_executable(<target> [SOURCES <file>...] [LIBS ...] [INCLUDES ...] [DEFINITIONS ...])
#     builds a test executable that several tests run (ursatz_test(... TARGET <target> ARGS ...)); registers nothing.
# ursatz_test_library(<target> SOURCES <file>... [LIBS ...] [INCLUDES ...] [DEFINITIONS ...])
#     builds a static library of test helpers with the same settings; LIBS and INCLUDES are PUBLIC.

set(URSATZ_TEST_FAST_TIMEOUT 120)
set(URSATZ_TEST_SLOW_TIMEOUT 1800)
set(URSATZ_FIXTURES_DIR "${CMAKE_CURRENT_LIST_DIR}/../tests/fixtures")
cmake_path(NORMAL_PATH URSATZ_FIXTURES_DIR)

function(_ursatz_test_properties test labels timeout environment properties)
    if(NOT timeout)
        if("slow" IN_LIST labels)
            set(timeout ${URSATZ_TEST_SLOW_TIMEOUT})
        else()
            set(timeout ${URSATZ_TEST_FAST_TIMEOUT})
        endif()
    endif()
    set_tests_properties(${test} PROPERTIES TIMEOUT ${timeout})
    if(labels)
        set_tests_properties(${test} PROPERTIES LABELS "${labels}")
    endif()
    if(environment)
        set_tests_properties(${test} PROPERTIES ENVIRONMENT "${environment}")
    endif()
    if(properties)
        set_tests_properties(${test} PROPERTIES ${properties})
    endif()
endfunction()

function(_ursatz_test_settings target scope libs includes definitions)
    target_compile_options(${target} PRIVATE -UNDEBUG)
    target_compile_definitions(${target} PRIVATE URSATZ_FIXTURES_DIR="${URSATZ_FIXTURES_DIR}" ${definitions})
    if(libs)
        target_link_libraries(${target} ${scope} ${libs})
    endif()
    if(includes)
        target_include_directories(${target} ${scope} ${includes})
    endif()
endfunction()

function(ursatz_test_executable target)
    cmake_parse_arguments(PARSE_ARGV 1 T "" "" "SOURCES;LIBS;INCLUDES;DEFINITIONS")
    if(T_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ursatz_test_executable(${target}): unknown arguments ${T_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT T_SOURCES)
        set(T_SOURCES ${target}.c)
    endif()
    add_executable(${target} ${T_SOURCES})
    _ursatz_test_settings(${target} PRIVATE "${T_LIBS}" "${T_INCLUDES}" "${T_DEFINITIONS}")
endfunction()

function(ursatz_test_library target)
    cmake_parse_arguments(PARSE_ARGV 1 T "" "" "SOURCES;LIBS;INCLUDES;DEFINITIONS")
    if(T_UNPARSED_ARGUMENTS OR NOT T_SOURCES)
        message(FATAL_ERROR "ursatz_test_library(${target}): needs SOURCES, no other arguments")
    endif()
    add_library(${target} STATIC ${T_SOURCES})
    _ursatz_test_settings(${target} PUBLIC "${T_LIBS}" "${T_INCLUDES}" "${T_DEFINITIONS}")
endfunction()

function(ursatz_test name)
    cmake_parse_arguments(PARSE_ARGV 1 T "VALGRIND" "TARGET;TIMEOUT;WORKING_DIRECTORY;VALGRIND_NAME"
        "SOURCES;COMMAND;ARGS;LIBS;INCLUDES;DEFINITIONS;LABELS;ENVIRONMENT;PROPERTIES")
    if(T_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ursatz_test(${name}): unknown arguments ${T_UNPARSED_ARGUMENTS}")
    endif()
    if("valgrind" IN_LIST T_LABELS AND NOT T_COMMAND)
        message(FATAL_ERROR "ursatz_test(${name}): use VALGRIND; only a COMMAND test sets the valgrind label itself")
    endif()

    if(name MATCHES "^test_")
        set(target ${name})
    else()
        set(target test_${name})
    endif()
    if((T_SOURCES OR T_LIBS OR T_INCLUDES OR T_DEFINITIONS) AND (T_TARGET OR T_COMMAND))
        message(FATAL_ERROR "ursatz_test(${name}): SOURCES, LIBS, INCLUDES or DEFINITIONS with TARGET or COMMAND")
    endif()
    if(T_TARGET)
        set(target ${T_TARGET})
    elseif(NOT T_COMMAND)
        if(NOT T_SOURCES)
            set(T_SOURCES ${target}.c)
        endif()
        ursatz_test_executable(${target} SOURCES ${T_SOURCES} LIBS ${T_LIBS} INCLUDES ${T_INCLUDES}
            DEFINITIONS ${T_DEFINITIONS})
    endif()

    if(T_COMMAND)
        set(command ${T_COMMAND} ${T_ARGS})
    else()
        set(command $<TARGET_FILE:${target}> ${T_ARGS})
    endif()
    set(workdir_args)
    if(T_WORKING_DIRECTORY)
        set(workdir_args WORKING_DIRECTORY ${T_WORKING_DIRECTORY})
    endif()
    add_test(NAME ${name} COMMAND ${command} ${workdir_args})
    _ursatz_test_properties(${name} "${T_LABELS}" "${T_TIMEOUT}" "${T_ENVIRONMENT}"
        "${T_PROPERTIES}")

    if(T_VALGRIND AND URSATZ_VALGRIND)
        if(T_COMMAND)
            message(FATAL_ERROR "ursatz_test(${name}): VALGRIND needs an executable, not COMMAND")
        endif()
        set(vname ${name}_valgrind)
        if(T_VALGRIND_NAME)
            set(vname ${T_VALGRIND_NAME})
        endif()
        add_test(NAME ${vname} COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS} $<TARGET_FILE:${target}> ${T_ARGS}
            ${workdir_args})
        set(vlabels ${T_LABELS})
        list(APPEND vlabels slow valgrind)
        list(REMOVE_DUPLICATES vlabels)
        _ursatz_test_properties(${vname} "${vlabels}" "" "${T_ENVIRONMENT}" "${T_PROPERTIES}")
    endif()
endfunction()
