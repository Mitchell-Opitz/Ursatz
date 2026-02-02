# Checks the --corpus flags and exit codes. Run with cmake -P and -DCLI=<ursatz-analyzer>, -DCORPORA_DIR=<test
# corpora>, -DFIXTURE=<midi file> and -DWORK_DIR=<scratch dir>.

foreach(var CLI CORPORA_DIR FIXTURE WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

set(MIXED "${CORPORA_DIR}/mixed")
set(OUT "${WORK_DIR}/out")

# check_case(<label> <expected exit code> <cli args>...); sets last_output to the CLI's standard output.
function(check_case label expected_exit)
    execute_process(COMMAND "${CLI}" ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_QUIET)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}")
    endif()
    set(last_output "${output}" PARENT_SCOPE)
    message(STATUS "${label}: ok")
endfunction()

function(expect_file label path present)
    if(present AND NOT EXISTS "${path}")
        message(FATAL_ERROR "${label}: expected ${path}")
    elseif(NOT present AND EXISTS "${path}")
        message(FATAL_ERROR "${label}: unexpected ${path}")
    endif()
endfunction()

function(expect_line label text)
    if(NOT last_output MATCHES "${text}")
        message(FATAL_ERROR "${label}: output lacks '${text}':\n${last_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
check_case("corpus" 0 --corpus "${MIXED}" --out "${OUT}")
expect_line("corpus" "analyzed Bach 114.mid\n")
expect_line("corpus" "analyzed sub/field-1.musicxml\n")
expect_line("corpus" "failed broken.mid: import failed\n")
expect_line("corpus" "2 analyzed, 0 skipped, 1 failed\n")
expect_file("corpus" "${OUT}/corpus-index.json" TRUE)
expect_file("corpus" "${OUT}/Bach 114.mid.analysis.json" TRUE)
expect_file("corpus" "${OUT}/Bach 114.mid.report.md" FALSE)
expect_file("corpus" "${OUT}/notes.txt.analysis.json" FALSE)

file(REMOVE_RECURSE "${OUT}")
check_case("--markdown" 0 --corpus "${MIXED}" --markdown --out "${OUT}")
expect_file("--markdown" "${OUT}/Bach 114.mid.report.md" TRUE)
expect_file("--markdown" "${OUT}/sub/field-1.musicxml.report.md" TRUE)

# Without --out the output goes to <input>/.ursatz-corpus.
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/in")
configure_file("${FIXTURE}" "${WORK_DIR}/in/piece.mid" COPYONLY)
execute_process(COMMAND "${CLI}" --corpus "${WORK_DIR}/in" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "default output: exit ${result}")
endif()
expect_file("default output" "${WORK_DIR}/in/.ursatz-corpus/corpus-index.json" TRUE)
# A second run does not walk its own output directory.
execute_process(COMMAND "${CLI}" --corpus "${WORK_DIR}/in" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_QUIET)
if(NOT output MATCHES "1 analyzed, 0 skipped, 0 failed")
    message(FATAL_ERROR "default output: second run walked its output:\n${output}")
endif()
message(STATUS "default output: ok")

# --resume skips what is unchanged.
file(REMOVE_RECURSE "${OUT}")
check_case("first run" 0 --corpus "${MIXED}" --out "${OUT}")
execute_process(COMMAND "${CLI}" --corpus "${MIXED}" --out "${OUT}" --resume
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_QUIET)
if(NOT result EQUAL 0 OR NOT output MATCHES "skipped Bach 114.mid\n" OR
   NOT output MATCHES "0 analyzed, 2 skipped, 1 failed")
    message(FATAL_ERROR "--resume: exit ${result}, output:\n${output}")
endif()
message(STATUS "--resume: ok")

# Usage errors exit 1.
check_case("--corpus without a directory" 1 --corpus)
check_case("--corpus and a file" 1 --corpus "${MIXED}" "${FIXTURE}")
check_case("--corpus and --json" 1 --corpus "${MIXED}" --json)
check_case("--corpus and --check-counterpoint" 1 --corpus "${MIXED}" --check-counterpoint 1)
check_case("--out without --corpus" 1 --out "${OUT}" "${FIXTURE}")
check_case("--resume without --corpus" 1 --resume "${FIXTURE}")

# A run that cannot proceed exits 2.
check_case("missing input directory" 2 --corpus "${WORK_DIR}/does-not-exist")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(WRITE "${WORK_DIR}/not-a-directory" "x")
check_case("output is a file" 2 --corpus "${MIXED}" --out "${WORK_DIR}/not-a-directory")
file(REMOVE_RECURSE "${OUT}")
file(MAKE_DIRECTORY "${OUT}")
file(WRITE "${OUT}/corpus-index.json" "{")
check_case("unreadable index" 2 --corpus "${MIXED}" --out "${OUT}" --resume)

file(REMOVE_RECURSE "${WORK_DIR}")
