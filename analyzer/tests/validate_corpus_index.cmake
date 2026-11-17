# Runs the CLI over the reference and mixed corpora and validates each corpus index against the schema. Run with
# cmake -P and -DCLI, -DPYTHON, -DVALIDATOR, -DSCHEMA, -DCORPORA_DIR and -DWORK_DIR. The test is registered only when
# python3 with jsonschema was found at configure time (-DPYTHON is that python3).

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
