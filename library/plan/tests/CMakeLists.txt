ursatz_test_includes()

# Plan format 6: three plans of format 5 (tests/fixtures) read and written, regions, derivation, the stamp.
ursatz_test(plan_format SOURCES test_plan_format.c LIBS ursatz_plan VALGRIND
    DEFINITIONS PLAN_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# The helpers the plan's files share: meters, cadences, lanes, keys, spans.
ursatz_test(plan_common SOURCES test_plan_common.c LIBS ursatz_plan VALGRIND)

# Resizing a lane span.
ursatz_test(plan_lane_resize SOURCES test_plan_lane_resize.c LIBS ursatz_plan VALGRIND)

# The section description: vocabulary, codec, rules and the member table against the schema fragment. The test writes
# plan_section_written_<use>_<n>.json in its build directory; plan_section_schema validates them against the fragment's profiles.
# The valgrind run writes the same files, so the two share a lock.
ursatz_test(plan_section LIBS ursatz_plan ursatz_test_support VALGRIND
    DEFINITIONS PLAN_SECTION_SCHEMA="${CMAKE_SOURCE_DIR}/library/plan/schema/section.schema.json"
    PROPERTIES FIXTURES_SETUP plan_section_files RESOURCE_LOCK plan_section_files)

# Needs python3 with jsonschema (cmake/UrsatzTools.cmake).
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(plan_section_schema COMMAND ${CMAKE_COMMAND} -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DFRAGMENT=${CMAKE_SOURCE_DIR}/library/plan/schema/section.schema.json
                     -DWRITTEN_DIR=${CMAKE_CURRENT_BINARY_DIR} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/plan_section_schema
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_section_schema.cmake
        LABELS schema
        PROPERTIES FIXTURES_REQUIRED plan_section_files RESOURCE_LOCK plan_section_files)
endif()
