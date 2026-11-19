ursatz_test_includes()

# The reading codec (R-629). The test writes reading_written_<name>.json in its build directory; reading_schema validates them against
# the schema. The valgrind run writes the same files, so the two share a lock.
ursatz_test(reading_json SOURCES test_reading_json.c LIBS ursatz_reading VALGRIND
    PROPERTIES FIXTURES_SETUP reading_files RESOURCE_LOCK reading_files)

# Needs python3 with jsonschema (cmake/UrsatzTools.cmake).
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(reading_schema COMMAND ${CMAKE_COMMAND} -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DSCHEMA=${CMAKE_SOURCE_DIR}/library/reading/schema/reading.v1.schema.json
                     -DWRITTEN_DIR=${CMAKE_CURRENT_BINARY_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_reading_schema.cmake
        LABELS schema
        PROPERTIES FIXTURES_REQUIRED reading_files RESOURCE_LOCK reading_files)
endif()
