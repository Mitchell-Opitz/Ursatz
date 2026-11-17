# Writes the analysis document for every corpus fixture with the CLI and
# validates each against the schema. Run with cmake -P and -DCLI, -DPYTHON,
# -DVALIDATOR, -DSCHEMA, -DFIXTURE_DIR and -DWORK_DIR. The test is registered only when python3 with jsonschema
# was found at configure time (-DPYTHON is that python3). With -DONLY=<file name> only that fixture is validated.

if(ONLY)
    set(fixtures "${FIXTURE_DIR}/${ONLY}")
    if(NOT EXISTS "${fixtures}")
        message(FATAL_ERROR "no fixture ${fixtures}")
    endif()
else()
    file(GLOB fixtures "${FIXTURE_DIR}/*.mid")
endif()
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
