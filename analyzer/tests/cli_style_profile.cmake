# Checks the --style-profile and --compare-styles flags and exit codes. Run with cmake -P and -DCLI=<ursatz-analyzer>,
# -DCORPORA_DIR=<test corpora> and -DWORK_DIR=<scratch dir>.

foreach(var CLI CORPORA_DIR WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

# check_case(<label> <expected exit code> <cli args>...); sets last_output to the CLI's standard output.
function(check_case label expected_exit)
    execute_process(COMMAND "${CLI}" ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_QUIET)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}")
    endif()
    set(last_output "${output}" PARENT_SCOPE)
    message(STATUS "${label}: ok")
endfunction()

function(expect_line label text)
    if(NOT last_output MATCHES "${text}")
        message(FATAL_ERROR "${label}: output lacks '${text}':\n${last_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
set(REFERENCE "${WORK_DIR}/reference")
set(SONATINAS "${WORK_DIR}/sonatinas")
check_case("corpus reference" 0 --corpus "${CORPORA_DIR}/reference" --out "${REFERENCE}")
check_case("corpus sonatinas" 0 --corpus "${CORPORA_DIR}/sonatinas" --out "${SONATINAS}")

# Profile: default output file, then --out.
check_case("--style-profile" 0 --style-profile "${REFERENCE}" --name reference)
expect_line("--style-profile" "from 5 pieces")
if(NOT EXISTS "${REFERENCE}/style-model.json")
    message(FATAL_ERROR "--style-profile: expected ${REFERENCE}/style-model.json")
endif()
check_case("--style-profile --out" 0 --style-profile "${SONATINAS}" --name sonatinas --out "${WORK_DIR}/sonatinas.json")
if(NOT EXISTS "${WORK_DIR}/sonatinas.json")
    message(FATAL_ERROR "--style-profile --out: expected ${WORK_DIR}/sonatinas.json")
endif()

# Profile usage errors and run errors.
check_case("no --name" 1 --style-profile "${REFERENCE}")
check_case("--name alone" 1 --name reference)
check_case("empty --name" 1 --style-profile "${REFERENCE}" --name "")
check_case("with a file" 1 --style-profile "${REFERENCE}" --name reference "${CORPORA_DIR}/reference/Field 1.mid")
check_case("with --corpus" 1 --style-profile "${REFERENCE}" --name reference --corpus "${CORPORA_DIR}/reference")
check_case("not a corpus output" 2 --style-profile "${CORPORA_DIR}/reference" --name reference)
check_case("missing dir" 2 --style-profile "${WORK_DIR}/nothing" --name reference)

# Compare: the same model is 0 everywhere; two different models are not.
check_case("--compare-styles same" 0 --compare-styles "${REFERENCE}/style-model.json" "${REFERENCE}/style-model.json")
expect_line("--compare-styles same" "overall 0.0000 \\(0/1\\)\n")
expect_line("--compare-styles same" "\nharmony 0.0000 \\(0/1\\)\n")
expect_line("--compare-styles same" "\n  cadence-type 0.0000 \\(0/1\\)")
check_case("--compare-styles differ" 0 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/sonatinas.json")
if(last_output MATCHES "overall 0.0000")
    message(FATAL_ERROR "--compare-styles differ: reference and sonatinas are at distance 0")
endif()
check_case("--format json" 0 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/sonatinas.json" --format json)
expect_line("--format json" "^\\{\n  \"overall\": \\{\"fraction\": ")
expect_line("--format json" "\"category\": \"cadence-type\"")
check_case("--format text" 0 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/sonatinas.json" --format text)
expect_line("--format text" "^overall ")

# Compare usage errors and run errors.
check_case("one model" 1 --compare-styles "${REFERENCE}/style-model.json")
check_case("bad format" 1 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/sonatinas.json" --format xml)
check_case("--format alone" 1 --format json)
check_case("compare with --corpus" 1 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/sonatinas.json" --corpus "${CORPORA_DIR}/reference")
check_case("compare with --name" 1 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/sonatinas.json" --name x)
check_case("unreadable first" 2 --compare-styles "${WORK_DIR}/nothing.json" "${REFERENCE}/style-model.json")
check_case("unreadable second" 2 --compare-styles "${REFERENCE}/style-model.json" "${WORK_DIR}/nothing.json")
check_case("not a model" 2 --compare-styles "${REFERENCE}/corpus-index.json" "${REFERENCE}/style-model.json")

file(REMOVE_RECURSE "${WORK_DIR}")
