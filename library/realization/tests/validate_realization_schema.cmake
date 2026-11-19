# Validates the realization documents test_realization_json wrote (realization_written_<name>.json) against
# library/realization/schema/realization.v1.schema.json. Run with cmake -P and -DPYTHON, -DVALIDATOR, -DSCHEMA and -DWRITTEN_DIR. The test
# is registered only when python3 with jsonschema was found at configure time (-DPYTHON is that python3).

file(GLOB documents "${WRITTEN_DIR}/realization_written_*.json")
if(NOT documents)
    message(FATAL_ERROR "no realization documents in ${WRITTEN_DIR}")
endif()
execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" ${documents} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "a realization document does not match the schema")
endif()
list(LENGTH documents count)
message(STATUS "${count} realization documents: valid")
