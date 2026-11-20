# Checks which files ursatz analyze writes: one analysis document next to the input, and no report. Run with
# cmake -P and -DCLI=<ursatz> -DFIXTURE=<midi file> -DWORK_DIR=<scratch dir>.

foreach(var CLI FIXTURE WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

set(INPUT "${WORK_DIR}/piece.mid")
set(DOCUMENT "${WORK_DIR}/piece.analysis.json")

# check_case(<label> <expected exit code> <document expected> <stderr pattern, or "" for none> <analyze args>...)
function(check_case label expected_exit expect_document stderr_pattern)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    configure_file("${FIXTURE}" "${INPUT}" COPYONLY)

    execute_process(COMMAND "${CLI}" analyze ${ARGN} RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}")
    endif()

    if(NOT stderr_pattern STREQUAL "" AND NOT error MATCHES "${stderr_pattern}")
        message(FATAL_ERROR "${label}: expected '${stderr_pattern}' on stderr, got: ${error}")
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

check_case("default" 0 TRUE "" "${INPUT}")
check_case("--json is gone" 1 FALSE "unknown option '--json'" --json "${INPUT}")
check_case("flag after path" 0 TRUE "" "${INPUT}" --meter 4/4)
check_case("--markdown is gone" 1 FALSE "unknown option '--markdown'" --markdown "${INPUT}")
check_case("unknown flag" 1 FALSE "unknown option '--bogus'" --bogus "${INPUT}")
check_case("missing path" 1 FALSE "analyze needs an input file" --meter 4/4)
check_case("two paths" 1 FALSE "analyze takes 1 input" "${INPUT}" "${INPUT}")
check_case("a flag without its value" 1 FALSE "--meter needs a value" "${INPUT}" --meter)
check_case("the output of another command" 1 FALSE "unknown option '--corpus'" --corpus "${INPUT}")

file(REMOVE_RECURSE "${WORK_DIR}")
