# Runs the CLI over the reference and mixed corpora and validates each corpus index against the schema. Run with
# cmake -P and -DCLI, -DPYTHON, -DHAVE_JSONSCHEMA, -DVALIDATOR, -DSCHEMA, -DCORPORA_DIR and -DWORK_DIR.
# Prints a SKIPPED line (matched by the test's SKIP_REGULAR_EXPRESSION) when python3 with jsonschema was not found at
# configure time.

if(NOT HAVE_JSONSCHEMA)
    message("SKIPPED: python3 with the jsonschema module was not found at configure time")
    return()
endif()

foreach(corpus reference mixed)
    file(REMOVE_RECURSE "${WORK_DIR}")
    execute_process(COMMAND "${CLI}" --corpus "${CORPORA_DIR}/${corpus}" --out "${WORK_DIR}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${corpus}: CLI exited ${result}")
    endif()
    execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" "${WORK_DIR}/corpus-index.json"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${corpus}: corpus index does not match the schema")
    endif()
    message(STATUS "${corpus}: valid")
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
