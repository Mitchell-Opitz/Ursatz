# Checks the --link-motives flag over a corpus run of the tests/links fixtures: text and JSON output, the cache, the
# options and the exit codes. Run with cmake -P and -DCLI=<ursatz-analyzer>, -DFIXTURE_DIR=<tests/links>,
# -DPYTHON, -DHAVE_JSONSCHEMA, -DVALIDATOR, -DLINKS_SCHEMA, -DCACHE_SCHEMA and -DWORK_DIR=<scratch dir>.

foreach(var CLI FIXTURE_DIR WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

# check_case(<label> <expected exit code> <cli args>...); sets last_output and last_error to the CLI's streams.
function(check_case label expected_exit)
    execute_process(COMMAND "${CLI}" ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}\n${output}${error}")
    endif()
    set(last_output "${output}" PARENT_SCOPE)
    set(last_error "${error}" PARENT_SCOPE)
    message(STATUS "${label}: ok")
endfunction()

function(expect_output label text)
    if(NOT last_output MATCHES "${text}")
        message(FATAL_ERROR "${label}: output lacks '${text}':\n${last_output}")
    endif()
endfunction()

function(expect_no_output label text)
    if(last_output MATCHES "${text}")
        message(FATAL_ERROR "${label}: output has '${text}':\n${last_output}")
    endif()
endfunction()

# Writes the CLI's JSON output to a file and validates it against a schema (skipped without jsonschema).
function(validate label schema file)
    if(HAVE_JSONSCHEMA)
        execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${schema}" "${file}" RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${label}: ${file} does not match ${schema}")
        endif()
        message(STATUS "${label}: valid")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
set(OUT "${WORK_DIR}/out")
check_case("corpus" 0 --corpus "${FIXTURE_DIR}" --out "${OUT}")

# Text: a links to b and to d, b to d, c to none; one group of three.
check_case("--link-motives" 0 --link-motives "${OUT}")
expect_output("--link-motives" "a.musicxml m.1 b.1 \\[1 3 5 4 3\\] -> b.musicxml m.1 b.1 \\[1 3 5 4 3\\]: augment ratio=2:1 \\(1/5\\)")
expect_output("--link-motives" "a.musicxml m.1 b.1 \\[1 3 5 4 3\\] -> d.musicxml m.1 b.1 \\[1 6 4 5 6\\]: invert tonal=1 \\(1/3\\)")
expect_output("--link-motives" "b.musicxml m.1 b.1 \\[1 3 5 4 3\\] -> d.musicxml m.1 b.1 \\[1 6 4 5 6\\]: invert tonal=1 then diminish ratio=1:2")
expect_output("--link-motives" "group of 3: a.musicxml #0, b.musicxml #0, d.musicxml #0\n")
expect_no_output("--link-motives" "c.musicxml")
expect_output("--link-motives" "compared 3, cached 0 of 3 pairs\n")
if(NOT EXISTS "${OUT}/motive-link-cache.json")
    message(FATAL_ERROR "--link-motives: expected ${OUT}/motive-link-cache.json")
endif()
validate("default cache" "${CACHE_SCHEMA}" "${OUT}/motive-link-cache.json")

# The cache is used, and written back.
check_case("--link-motives again" 0 --link-motives "${OUT}")
expect_output("--link-motives again" "compared 0, cached 3 of 3 pairs\n")
expect_output("--link-motives again" "group of 3: ")

# JSON: valid against the schema, the same links, the counts of the cache.
set(JSON_FILE "${WORK_DIR}/links.json")
execute_process(COMMAND "${CLI}" --link-motives "${OUT}" --format json RESULT_VARIABLE result OUTPUT_FILE "${JSON_FILE}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "--format json: exit ${result}")
endif()
validate("--format json" "${LINKS_SCHEMA}" "${JSON_FILE}")
file(READ "${JSON_FILE}" json)
string(JSON schema GET "${json}" schema)
string(JSON link_count LENGTH "${json}" links)
string(JSON group_count LENGTH "${json}" groups)
string(JSON cached GET "${json}" cached)
string(JSON first_steps GET "${json}" links 0 steps 0)
if(NOT schema STREQUAL "ursatz-motive-links" OR NOT link_count EQUAL 3 OR NOT group_count EQUAL 1 OR NOT cached EQUAL 3 OR
   NOT first_steps STREQUAL "augment ratio=2:1")
    message(FATAL_ERROR "--format json: unexpected content:\n${json}")
endif()
message(STATUS "--format json: ok")

# --cache: another file, its own counts; --depth and --candidates change the options, which empties a cache made for others.
set(OWN_CACHE "${WORK_DIR}/own-cache.json")
check_case("--cache" 0 --link-motives "${OUT}" --cache "${OWN_CACHE}")
expect_output("--cache" "compared 3, cached 0 of 3 pairs\n")
if(NOT EXISTS "${OWN_CACHE}")
    message(FATAL_ERROR "--cache: expected ${OWN_CACHE}")
endif()
validate("--cache" "${CACHE_SCHEMA}" "${OWN_CACHE}")
check_case("--depth 1" 0 --link-motives "${OUT}" --cache "${OWN_CACHE}" --depth 1)
expect_output("--depth 1" "compared 3, cached 0 of 3 pairs\n")
expect_output("--depth 1" "-> d.musicxml m.1 b.1 \\[1 6 4 5 6\\]: invert tonal=1 \\(1/3\\)")
expect_no_output("--depth 1" "then")
check_case("--candidates 1" 0 --link-motives "${OUT}" --candidates 1)
expect_output("--candidates 1" "of [0-9]+ pairs\n")

# Usage errors.
check_case("no directory" 1 --link-motives)
check_case("--depth 0" 1 --link-motives "${OUT}" --depth 0)
check_case("--depth 7" 1 --link-motives "${OUT}" --depth 7)
check_case("--candidates 0" 1 --link-motives "${OUT}" --candidates 0)
check_case("--candidates text" 1 --link-motives "${OUT}" --candidates many)
check_case("--cache alone" 1 --cache "${OWN_CACHE}")
check_case("--depth alone" 1 --depth 2)
check_case("with --corpus" 1 --link-motives "${OUT}" --corpus "${FIXTURE_DIR}")
check_case("with --similar" 1 --link-motives "${OUT}" --similar "${OUT}")
check_case("with a file" 1 --link-motives "${OUT}" "${FIXTURE_DIR}/a.musicxml")

# Run errors: no corpus there, a document without features, a cache that is not one.
check_case("missing dir" 2 --link-motives "${WORK_DIR}/nothing")
check_case("not a corpus output" 2 --link-motives "${FIXTURE_DIR}")
set(OLD "${WORK_DIR}/old")
file(COPY "${OUT}/" DESTINATION "${OLD}" PATTERN "motive-link-cache.json" EXCLUDE)
file(READ "${OLD}/b.musicxml.analysis.json" document)
string(JSON document REMOVE "${document}" motives 0 features)
file(WRITE "${OLD}/b.musicxml.analysis.json" "${document}")
check_case("document without features" 2 --link-motives "${OLD}")
if(NOT last_error MATCHES "b.musicxml" OR NOT last_error MATCHES "run the corpus again")
    message(FATAL_ERROR "document without features: the message does not name the piece:\n${last_error}")
endif()
file(WRITE "${WORK_DIR}/bad-cache.json" "{\"schema\": \"other\"}")
check_case("bad cache" 2 --link-motives "${OUT}" --cache "${WORK_DIR}/bad-cache.json")

file(REMOVE_RECURSE "${WORK_DIR}")
