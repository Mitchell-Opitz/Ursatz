# ursatz knowledge (R-765): find, show and check on the built-in entries, and the refusals.
# Run with: cmake -DURSATZ=<ursatz> -DWORK=<working directory> -P cli_knowledge.cmake

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

# find: one line each, the alias counts, --kind limits.
expect_status(0 knowledge find mario)
expect_match("${OUT}" "omt.schema.aeolian-cadence  schema/progression  Aeolian cadence\n")
expect_status(0 knowledge find shuttle --kind schema)
expect_match("${OUT}" "omt.schema.dorian-shuttle  schema/progression  Dorian shuttle\n")
expect_match("${OUT}" "omt.schema.subtonic-shuttle")
expect_status(0 knowledge find "dorian cadence")
expect_match("${OUT}" "persichetti.vocabulary.dorian-cadence  vocabulary/cadence-kind")
expect_status(0 knowledge find zzzzzz)
if(NOT "${OUT}" STREQUAL "")
    message(FATAL_ERROR "find with no match prints nothing, got: ${OUT}")
endif()
expect_status(1 knowledge find shuttle --kind nothing)
expect_match("${ERR}" "no entry has the kind 'nothing'")

# show: Ursatz Text by default, the entry as loaded with --format json.
expect_status(0 knowledge show omt.vocabulary.cadential)
expect_match("${OUT}" "ursatz-text 1 knowledge-entry\nid omt.vocabulary.cadential\n")
expect_status(0 knowledge show omt.vocabulary.cadential --format json)
expect_match("${OUT}" "\"id\": \"omt.vocabulary.cadential\"")
expect_status(1 knowledge show omt.vocabulary.nothing)
expect_match("${ERR}" "no knowledge entry 'omt.vocabulary.nothing'")

# check: a document that loads, one the loader refuses, a file that is no document.
file(WRITE "${WORK}/good.json" "{\"schema\": \"ursatz-knowledge\", \"schema_version\": 1, \"source\": \"user\", \"entries\": []}")
expect_status(0 knowledge check good.json)
expect_match("${OUT}" "ok: 0 entries\n")
file(WRITE "${WORK}/broken.json" "{\"schema\": \"ursatz-knowledge\", \"schema_version\": 1, \"source\": \"user\", \"entries\": [{\"id\": \"user.voicing.x\", \"kind\": \"voicing\", \"name\": \"x\"}]}")
expect_status(2 knowledge check broken.json)
expect_match("${ERR}" "user.voicing.x")
file(WRITE "${WORK}/list.json" "[]")
expect_status(2 knowledge check list.json)
expect_status(1 knowledge check missing.json)

# usage: an unknown action, a missing word, --kind with show.
expect_status(1 knowledge bogus x)
expect_match("${ERR}" "unknown action 'bogus'; use find, show or check")
expect_status(1 knowledge find)
expect_status(1 knowledge show omt.vocabulary.cadential --kind schema)
