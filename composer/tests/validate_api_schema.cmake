# Validates the JSON the API tests wrote against schema/api/*.schema.json: <kind>_response_*.json in WRITTEN_DIR is checked
# with <kind>.schema.json. Run with cmake -P and -DKINDS (a comma-separated list), -DPYTHON, -DHAVE_JSONSCHEMA, -DVALIDATOR, -DSCHEMA_DIR and -DWRITTEN_DIR.
# Prints a SKIPPED line (matched by the test's SKIP_REGULAR_EXPRESSION) when python3 with jsonschema was not found at
# configure time.

if(NOT HAVE_JSONSCHEMA)
    message("SKIPPED: python3 with the jsonschema module was not found at configure time")
    return()
endif()

string(REPLACE "," ";" KINDS "${KINDS}")
set(checked 0)
foreach(kind IN LISTS KINDS)
    # A kind with requests has <kind>.request.schema.json and <kind>.response.schema.json; any other is <kind>.schema.json.
    set(response_schema "${SCHEMA_DIR}/${kind}.schema.json")
    if(EXISTS "${SCHEMA_DIR}/${kind}.response.schema.json")
        set(response_schema "${SCHEMA_DIR}/${kind}.response.schema.json")
    endif()
    file(GLOB written "${WRITTEN_DIR}/api_${kind}_response_*.json")
    if(NOT written)
        message(FATAL_ERROR "no ${kind} responses were written to ${WRITTEN_DIR}")
    endif()
    execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${response_schema}" ${written} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "a ${kind} response does not match ${response_schema}")
    endif()
    list(LENGTH written count)
    math(EXPR checked "${checked} + ${count}")
    if(EXISTS "${SCHEMA_DIR}/${kind}.request.schema.json")
        file(GLOB requests "${WRITTEN_DIR}/api_${kind}_request_*.json")
        if(NOT requests)
            message(FATAL_ERROR "no ${kind} requests were written to ${WRITTEN_DIR}")
        endif()
        execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA_DIR}/${kind}.request.schema.json" ${requests}
                        RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "a ${kind} request does not match schema/api/${kind}.request.schema.json")
        endif()
        list(LENGTH requests count)
        math(EXPR checked "${checked} + ${count}")
    endif()
endforeach()
message(STATUS "${checked} API responses: valid")
