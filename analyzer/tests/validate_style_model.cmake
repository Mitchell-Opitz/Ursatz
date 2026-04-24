# Runs the CLI over the reference corpus, writes a style model and validates it against the schema. Run with
# cmake -P and -DCLI, -DPYTHON, -DHAVE_JSONSCHEMA, -DVALIDATOR, -DSCHEMA, -DCORPORA_DIR and -DWORK_DIR.
# Prints a SKIPPED line (matched by the test's SKIP_REGULAR_EXPRESSION) when python3 with jsonschema was not found at
# configure time.

if(NOT HAVE_JSONSCHEMA)
    message("SKIPPED: python3 with the jsonschema module was not found at configure time")
    return()
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
execute_process(COMMAND "${CLI}" --corpus "${CORPORA_DIR}/reference" --out "${WORK_DIR}"
                RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "corpus run exited ${result}")
endif()
execute_process(COMMAND "${CLI}" --style-profile "${WORK_DIR}" --name reference
                RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "style profile exited ${result}")
endif()
execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" "${WORK_DIR}/style-model.json" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "style model does not match the schema")
endif()
message(STATUS "style model: valid")

file(REMOVE_RECURSE "${WORK_DIR}")
