# Checks the CLI's --check-counterpoint mode and its exit codes. Run with cmake -P and -DCLI=<ursatz-analyzer>
# -DFIXTURE_DIR=<dir of the counterpoint fixtures> -DNOT_TWO_VOICES=<a file with more than two voices>
# -DWORK_DIR=<scratch dir>.

foreach(var CLI FIXTURE_DIR NOT_TWO_VOICES WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

# check_case(<label> <source> <expected exit code> <expected output files> <stderr must contain> <cli args>...)
# Copies the source to a clean work dir and runs the CLI on it with the arguments before the path.
function(check_case label source expected_exit expected_outputs expected_error)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    get_filename_component(name "${source}" NAME)
    configure_file("${source}" "${WORK_DIR}/${name}" COPYONLY)

    execute_process(COMMAND "${CLI}" ${ARGN} "${WORK_DIR}/${name}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}: ${error}")
    endif()
    if(NOT expected_error STREQUAL "" AND NOT error MATCHES "${expected_error}")
        message(FATAL_ERROR "${label}: expected '${expected_error}' on stderr, got: ${error}")
    endif()
    file(GLOB outputs "${WORK_DIR}/*.report.md" "${WORK_DIR}/*.analysis.json")
    list(LENGTH outputs output_count)
    if(NOT output_count EQUAL expected_outputs)
        message(FATAL_ERROR "${label}: expected ${expected_outputs} output files, found ${output_count}")
    endif()
    message(STATUS "${label}: ok")
endfunction()

set(VALID "${FIXTURE_DIR}/first-species-valid.musicxml")

check_case("two voices" "${VALID}" 0 2 "" --check-counterpoint 1)
check_case("two voices, cantus upper" "${VALID}" 0 2 "" --check-counterpoint 1 --cantus upper)
check_case("two voices, fifth species shape error" "${VALID}" 0 2 "" --check-counterpoint 5 --cantus lower)
check_case("more than two voices" "${NOT_TWO_VOICES}" 2 2 "counterpoint check needs two voices"
           --check-counterpoint 1)
check_case("species out of range" "${VALID}" 1 0 "Usage" --check-counterpoint 6)
check_case("species missing" "${VALID}" 1 0 "Usage" --check-counterpoint)
check_case("cantus without the check" "${VALID}" 1 0 "Usage" --cantus upper)
check_case("bad cantus" "${VALID}" 1 0 "Usage" --check-counterpoint 1 --cantus middle)
