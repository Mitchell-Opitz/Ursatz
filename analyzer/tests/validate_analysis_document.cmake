# Writes the analysis document for every corpus fixture with the CLI and
# validates each against the schema. Run with cmake -P and -DCLI, -DPYTHON,
# -DHAVE_JSONSCHEMA, -DVALIDATOR, -DSCHEMA, -DFIXTURE_DIR and -DWORK_DIR.
# Prints a SKIPPED line (matched by the test's SKIP_REGULAR_EXPRESSION)
# when python3 with jsonschema was not found at configure time.

if(NOT HAVE_JSONSCHEMA)
    message("SKIPPED: python3 with the jsonschema module was not found at configure time")
    return()
endif()

file(GLOB fixtures "${FIXTURE_DIR}/*.mid")
if(NOT fixtures)
    message(FATAL_ERROR "no fixtures found in ${FIXTURE_DIR}")
endif()

foreach(fixture IN LISTS fixtures)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    configure_file("${fixture}" "${WORK_DIR}/piece.mid" COPYONLY)

    execute_process(COMMAND "${CLI}" --json "${WORK_DIR}/piece.mid" RESULT_VARIABLE result OUTPUT_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${fixture}: CLI exited ${result}")
    endif()

    execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" "${WORK_DIR}/piece.analysis.json"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${fixture}: analysis document does not match the schema")
    endif()
    message(STATUS "${fixture}: valid")
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
