# Checks which files each CLI output flag writes: one analysis document next to the input, and no report. Run with
# cmake -P and -DCLI=<ursatz-analyzer> -DFIXTURE=<midi file> -DWORK_DIR=<scratch dir>.

foreach(var CLI FIXTURE WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

set(INPUT "${WORK_DIR}/piece.mid")
set(DOCUMENT "${WORK_DIR}/piece.analysis.json")

# check_case(<label> <expected exit code> <document expected> <cli args>...)
function(check_case label expected_exit expect_document)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    configure_file("${FIXTURE}" "${INPUT}" COPYONLY)

    execute_process(COMMAND "${CLI}" ${ARGN} RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}")
    endif()

    file(GLOB written "${WORK_DIR}/*.report.md" "${WORK_DIR}/*.analysis.json")
    if(expect_document)
        if(NOT written STREQUAL "${DOCUMENT}")
            message(FATAL_ERROR "${label}: expected only ${DOCUMENT} to be written, found: ${written}")
        endif()
    elseif(written)
        message(FATAL_ERROR "${label}: expected nothing to be written, found: ${written}")
    endif()
    message(STATUS "${label}: ok")
endfunction()

check_case("default" 0 TRUE "${INPUT}")
check_case("--json" 0 TRUE --json "${INPUT}")
check_case("flag after path" 0 TRUE "${INPUT}" --json)
check_case("--markdown is gone" 1 FALSE --markdown "${INPUT}")
check_case("unknown flag" 1 FALSE --bogus "${INPUT}")
check_case("missing path" 1 FALSE --json)
check_case("two paths" 1 FALSE "${INPUT}" "${INPUT}")

file(REMOVE_RECURSE "${WORK_DIR}")
