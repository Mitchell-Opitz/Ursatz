# Checks the --similar flag, its output and its exit codes, and validates the JSON output against the schema when
# python3 with jsonschema was found at configure time. Run with cmake -P and -DCLI=<ursatz-analyzer>,
# -DCORPORA_DIR=<test corpora>, -DMOTIVE=<a four-note MIDI motive>, -DPYTHON, -DHAVE_JSONSCHEMA, -DVALIDATOR, -DSCHEMA
# and -DWORK_DIR=<scratch dir>.

foreach(var CLI CORPORA_DIR MOTIVE WORK_DIR SCHEMA)
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

function(expect_output label text)
    if(NOT last_output MATCHES "${text}")
        message(FATAL_ERROR "${label}: output lacks '${text}':\n${last_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
set(REFERENCE "${WORK_DIR}/reference")
check_case("corpus reference" 0 --corpus "${CORPORA_DIR}/reference" --out "${REFERENCE}")

# A motive file: text, then JSON.
check_case("--motive" 0 --similar "${REFERENCE}" --motive "${MOTIVE}")
expect_output("--motive" "Mozart 545.mid m\\.1 b\\.1")
expect_output("--motive" "interval [0-9]+/[0-9]+ contour [0-9]+/[0-9]+ rhythm [0-9]+/[0-9]+")
check_case("--motive --max" 0 --similar "${REFERENCE}" --motive "${MOTIVE}" --max 2)
string(REGEX MATCHALL "\n" lines "${last_output}")
list(LENGTH lines line_count)
if(NOT line_count EQUAL 2)
    message(FATAL_ERROR "--max 2: expected 2 lines, got ${line_count}:\n${last_output}")
endif()
check_case("--motive --format json" 0 --similar "${REFERENCE}" --motive "${MOTIVE}" --format json)
expect_output("--motive --format json" "\"schema\": \"ursatz-similar-material\"")
expect_output("--motive --format json" "\"kind\": \"motive\"")
file(WRITE "${WORK_DIR}/motive.json" "${last_output}")

# A progression.
check_case("--progression" 0 --similar "${REFERENCE}" --progression "I IV V I" --max 100)
expect_output("--progression" "Bach 114.mid m\\.")
expect_output("--progression" "numerals [0-9]/4")
check_case("--progression dashes" 0 --similar "${REFERENCE}" --progression "ii-V7-I" --format json)
expect_output("--progression dashes" "\"numerals\": \\[\"ii\", \"V7\", \"I\"\\]")
file(WRITE "${WORK_DIR}/progression.json" "${last_output}")

# No match is not an error.
check_case("no match" 0 --similar "${REFERENCE}" --progression "I IV V I" --min 1/1)
expect_output("no match" "^no match in 5 pieces\n")
check_case("no match json" 0 --similar "${REFERENCE}" --progression "I IV V I" --min 1/1 --format json)
file(WRITE "${WORK_DIR}/empty.json" "${last_output}")

if(HAVE_JSONSCHEMA)
    foreach(name motive progression empty)
        execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" "${WORK_DIR}/${name}.json" RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${name}.json does not match the schema")
        endif()
    endforeach()
    message(STATUS "similar material output: valid")
else()
    message(STATUS "similar material output: schema check skipped, python3 with jsonschema was not found")
endif()

# Usage errors (1).
check_case("no query" 1 --similar "${REFERENCE}")
check_case("both queries" 1 --similar "${REFERENCE}" --motive "${MOTIVE}" --progression "I V")
check_case("no corpus" 1 --motive "${MOTIVE}")
check_case("--min without --similar" 1 --min 1/2)
check_case("bad --min" 1 --similar "${REFERENCE}" --progression "I V" --min 3/2)
check_case("bad --min text" 1 --similar "${REFERENCE}" --progression "I V" --min half)
check_case("bad --max" 1 --similar "${REFERENCE}" --progression "I V" --max 0)
check_case("bad --format" 1 --similar "${REFERENCE}" --progression "I V" --format xml)
check_case("with a file" 1 --similar "${REFERENCE}" --progression "I V" "${MOTIVE}")
check_case("with --corpus" 1 --similar "${REFERENCE}" --progression "I V" --corpus "${CORPORA_DIR}/reference")
check_case("with --compare-styles" 1 --similar "${REFERENCE}" --progression "I V" --compare-styles a b)
check_case("one numeral" 1 --similar "${REFERENCE}" --progression "I")
check_case("not a numeral" 1 --similar "${REFERENCE}" --progression "I banana")

# Unreadable input (2).
check_case("missing corpus" 2 --similar "${WORK_DIR}/nothing" --progression "I V")
check_case("not a corpus output" 2 --similar "${CORPORA_DIR}/reference" --progression "I V")
check_case("missing motive" 2 --similar "${REFERENCE}" --motive "${WORK_DIR}/nothing.mid")
