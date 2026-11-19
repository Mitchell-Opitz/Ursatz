ursatz_test_includes()

ursatz_test(realization_lines SOURCES test_realization_lines.c LIBS ursatz_realization VALGRIND)
ursatz_test(realization_scoring SOURCES test_realization_scoring.c LIBS ursatz_realization VALGRIND)
ursatz_test(realization_movement SOURCES test_realization_movement.c LIBS ursatz_realization VALGRIND)

# The realization codec (R-627). The test writes realization_written_<name>.json in its build directory; realization_schema validates
# them against the schema. The valgrind run writes the same files, so the two share a lock.
ursatz_test(realization_json SOURCES test_realization_json.c LIBS ursatz_realization VALGRIND
    PROPERTIES FIXTURES_SETUP realization_files RESOURCE_LOCK realization_files)

# Needs python3 with jsonschema (cmake/UrsatzTools.cmake).
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(realization_schema COMMAND ${CMAKE_COMMAND} -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DSCHEMA=${CMAKE_SOURCE_DIR}/library/realization/schema/realization.v1.schema.json
                     -DWRITTEN_DIR=${CMAKE_CURRENT_BINARY_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_realization_schema.cmake
        LABELS schema
        PROPERTIES FIXTURES_REQUIRED realization_files RESOURCE_LOCK realization_files)
endif()
