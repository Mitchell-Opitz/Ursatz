# The tools some tests need, found once for the whole tree.
#
#   URSATZ_VALGRIND             valgrind, for the tests ursatz_test(... VALGRIND) adds (labels slow and valgrind)
#   URSATZ_VALGRIND_ARGS        the one memcheck flag set those tests use
#   URSATZ_JSONSCHEMA_PYTHON    a python3 that imports jsonschema, for the tests labelled schema
#   URSATZ_NODE                 node, for the GUI frontend tests
#   Python3_EXECUTABLE          python3, for the layer table check (required)
#
# A test that needs a missing tool is not registered. URSATZ_REQUIRE lists the tools a configure must find
# (valgrind, jsonschema, node): CI passes jsonschema and node, the nightly run all three, so a set never goes missing
# silently.

set(URSATZ_REQUIRE "" CACHE STRING "Tools the configure must find: any of valgrind, jsonschema, node")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

find_program(URSATZ_VALGRIND valgrind)
set(URSATZ_VALGRIND_ARGS --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect)

set(URSATZ_JSONSCHEMA_PYTHON "")
execute_process(COMMAND ${Python3_EXECUTABLE} -c "import jsonschema"
    RESULT_VARIABLE _ursatz_jsonschema_result OUTPUT_QUIET ERROR_QUIET)
if(_ursatz_jsonschema_result EQUAL 0)
    set(URSATZ_JSONSCHEMA_PYTHON ${Python3_EXECUTABLE})
endif()

find_program(URSATZ_NODE node)

foreach(tool IN LISTS URSATZ_REQUIRE)
    if(tool STREQUAL "valgrind")
        set(found ${URSATZ_VALGRIND})
    elseif(tool STREQUAL "jsonschema")
        set(found ${URSATZ_JSONSCHEMA_PYTHON})
    elseif(tool STREQUAL "node")
        set(found ${URSATZ_NODE})
    else()
        message(FATAL_ERROR "URSATZ_REQUIRE: unknown tool ${tool} (valgrind, jsonschema, node)")
    endif()
    if(NOT found)
        message(FATAL_ERROR "URSATZ_REQUIRE names ${tool}, which this configure did not find")
    endif()
endforeach()

foreach(tool valgrind jsonschema node)
    if(tool STREQUAL "valgrind")
        set(found ${URSATZ_VALGRIND})
    elseif(tool STREQUAL "jsonschema")
        set(found ${URSATZ_JSONSCHEMA_PYTHON})
    else()
        set(found ${URSATZ_NODE})
    endif()
    if(NOT found)
        message(STATUS "Ursatz tests: ${tool} not found, its tests are not registered")
    endif()
endforeach()
