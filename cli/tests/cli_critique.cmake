# ursatz critique (R-721): candidate files judged as one set, text and JSON, and a rated set judged pair by pair.
# Run with: cmake -DURSATZ=<ursatz> -DWORK=<working directory> -P cli_critique.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run)
    execute_process(COMMAND "${URSATZ}" ${ARGN} WORKING_DIRECTORY "${WORK}"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
    set(OUT "${out}" PARENT_SCOPE)
    set(ERR "${err}" PARENT_SCOPE)
    set(STATUS "${status}" PARENT_SCOPE)
endfunction()

function(expect_status expected)
    run(${ARGN})
    if(NOT STATUS EQUAL expected)
        message(FATAL_ERROR "ursatz ${ARGN}: exit ${STATUS}, expected ${expected}\n${OUT}\n${ERR}")
    endif()
    set(OUT "${OUT}" PARENT_SCOPE)
    set(ERR "${ERR}" PARENT_SCOPE)
endfunction()

function(expect_match text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "expected /${pattern}/ in:\n${text}")
    endif()
endfunction()

# Two motif samples and two theme samples (with their plans), made by listen.
expect_status(0 listen --what motifs --count 2 --seed 1 --out-dir motifs)
expect_status(0 listen --what themes --count 2 --seed 1 --plans --out-dir themes)

# The motif samples, one set: a score for each, in text.
expect_status(0 critique motifs/01.json motifs/02.json)
expect_match("${OUT}" "motifs/01.json  score -?[0-9]+/[0-9]+\n")
expect_match("${OUT}" "motifs/02.json  score -?[0-9]+/[0-9]+\n")
expect_match("${OUT}" "\n  bar [-0-9]+ beat [-0-9/]+  [a-z_]+  [^\n]+")

# The theme samples, in text and in JSON.
expect_status(0 critique themes/01.json themes/02.json)
expect_match("${OUT}" "themes/01.json  score ")
expect_match("${OUT}" "themes/02.json  score ")
expect_status(0 critique themes/01.json themes/02.json --format json)
string(JSON schema GET "${OUT}" schema)
string(JSON count LENGTH "${OUT}" candidates)
string(JSON rules LENGTH "${OUT}" candidates 0 evaluators)
if(NOT schema STREQUAL "ursatz-critique" OR NOT count EQUAL 2 OR NOT rules EQUAL 10)
    message(FATAL_ERROR "the JSON answer is not what the critique entry gives: ${OUT}")
endif()

# A motif and a theme together; the settings give a weight and reverse an evaluator.
file(WRITE "${WORK}/mine.json" "{\"critic\": {\"weights\": {\"leap_load\": 0}, \"reverse\": [\"wide_range\"]}}")
expect_status(0 critique motifs/01.json themes/01.json --settings mine.json --format json)
string(JSON weight GET "${OUT}" candidates 0 evaluators 2 weight)
string(JSON reversed GET "${OUT}" candidates 0 evaluators 7 reversed)
if(NOT weight EQUAL 0 OR NOT reversed)
    message(FATAL_ERROR "--settings did not reach the critic: ${OUT}")
endif()

# A theme sample made without --plans has nothing to judge; a missing file, bad format and bad settings are usage errors.
expect_status(0 listen --what themes --count 1 --seed 1 --out-dir bare)
expect_status(1 critique bare/01.json)
expect_match("${ERR}" "listen --plans")
expect_status(1 critique missing.json)
expect_match("${ERR}" "cannot read")
expect_status(1 critique motifs/01.json --format yaml)
expect_status(1 critique)
expect_status(1 critique motifs/01.json --settings missing.json)
expect_status(1 critique motifs/01.json --rated rated.json)

# A rated set over the four samples: two pairs, each side its own set.
file(WRITE "${WORK}/rated.json" "{\"schema\": \"ursatz-rated-set\", \"schema_version\": 1, \"pairs\": [
 {\"label\": \"m-1\", \"a\": \"motifs/01.json\", \"b\": \"motifs/02.json\", \"rating_a\": 2, \"rating_b\": 3.5, \"set_a\": \"m-a\", \"set_b\": \"m-b\"},
 {\"label\": \"m-2\", \"a\": \"motifs/02.json\", \"b\": \"motifs/01.json\", \"rating_a\": 3, \"rating_b\": 3, \"set_a\": \"m-a\", \"set_b\": \"m-b\"},
 {\"label\": \"t-1\", \"a\": \"themes/01.json\", \"b\": \"themes/02.json\", \"rating_a\": 4, \"rating_b\": 2.5, \"set_a\": \"t-a\", \"set_b\": \"t-b\"},
 {\"label\": \"t-2\", \"a\": \"themes/02.json\", \"b\": \"themes/01.json\", \"rating_a\": 1, \"rating_b\": 5, \"set_a\": \"t-a\", \"set_b\": \"t-b\"}]}")
expect_status(0 critique --rated rated.json)
foreach(label m-1 m-2 t-1 t-2)
    expect_match("${OUT}" "${label} +[0-9.]+ [0-9.]+  critic +-?[0-9.]+ +-?[0-9.]+ (match|miss|tie|unrated) +today (match|miss|tie|unrated)")
endforeach()
expect_match("${OUT}" "m-2 +3.0 3.0  critic +-?[0-9.]+ +-?[0-9.]+ unrated +today unrated")
expect_match("${OUT}" "critic: match [0-9]+  miss [0-9]+  tie [0-9]+  of 3\n")
expect_match("${OUT}" "today: match [0-9]+  miss [0-9]+  tie [0-9]+  of 3\n")
# today's ranking has no score for a motif: a tie
expect_match("${OUT}" "m-1 +2.0 3.5  critic +-?[0-9.]+ +-?[0-9.]+ [a-z]+ +today tie")
file(WRITE "${WORK}/not-rated.json" "{\"schema\": \"ursatz-plan\"}")
expect_status(1 critique --rated not-rated.json)
expect_match("${ERR}" "ursatz-rated-set")
expect_status(1 critique --rated missing.json)
