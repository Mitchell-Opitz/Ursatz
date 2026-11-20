# "Add a 4-bar phrase between A and B" with `ursatz apply` alone (P65 B7, R-666), as processes on a migrated copy of the layout 0 fixture folder: two section
# inserts and a compose, in the text of Ursatz Text commands.
# Run with: cmake -DURSATZ=<ursatz> -DDATA_V0=<fixture folder> -DWORK=<folder of its own> -P cli_text_apply.cmake

if(NOT URSATZ OR NOT DATA_V0 OR NOT WORK)
    message(FATAL_ERROR "give -DURSATZ, -DDATA_V0 and -DWORK")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${DATA_V0}/" DESTINATION "${WORK}/data")
set(DATA "${WORK}/data")
set(PIECE pce-2c8d54834a9c499f)

function(ursatz)
    execute_process(COMMAND "${URSATZ}" ${ARGN} WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(stdout "${out}" PARENT_SCOPE)
    set(out "${out}${err}" PARENT_SCOPE)
    set(status "${status}" PARENT_SCOPE)
endfunction()

macro(expect_status wanted what)
    if(NOT status EQUAL ${wanted})
        message(FATAL_ERROR "${what}: exit ${status}, expected ${wanted}:\n${out}")
    endif()
endmacro()

macro(expect_text pattern what)
    if(NOT out MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: the output does not match '${pattern}':\n${out}")
    endif()
endmacro()

ursatz(--data "${DATA}" migrate)
expect_status(0 "migrate")

file(WRITE "${WORK}/edit.txt"
"# add B, then a 4-bar phrase between A (s0) and B
section.insert item:${PIECE}@v2 after s0 id b bars 8 role body letter B
section.insert item:${PIECE}@v3 after s0 id T0 bars 4 role body letter T function transition
item.compose item:${PIECE}@v4 seed 1
")
ursatz(--data "${DATA}" apply "${WORK}/edit.txt" --wait)
expect_status(0 "apply of the edit")
expect_text("1  section.insert  ${PIECE}  journal [0-9]+  v3\n" "the first insert makes version 3")
expect_text("2  section.insert  ${PIECE}  journal [0-9]+  v4\n" "the second insert makes version 4")
expect_text("3  item.compose  ${PIECE}  journal 0  job [^ ]+  done  v5\n" "the compose is a job that finishes done and makes version 5")

ursatz(--data "${DATA}" show "item:${PIECE}" --view summary)
expect_status(0 "show the summary")
expect_text("ursatz-text 1 view summary item:${PIECE}@v5\n" "the summary is of version 5")
expect_text("\n  s0  bars 1-8  " "the summary lists the row s0")
expect_text("\n  T0  bars 9-12  role body  letter T  function transition  " "the summary lists T0, a transition, bars 9 to 12")
expect_text("\n  b  bars 13-20  role body  letter B  " "the summary lists b, bars 13 to 20")
string(FIND "${stdout}" "\n  T0  " at_t0)
string(FIND "${stdout}" "\n  b  " at_b)
if(at_t0 GREATER at_b)
    message(FATAL_ERROR "T0 is not between s0 and b:\n${stdout}")
endif()

ursatz(--data "${DATA}" history --limit 3)
expect_status(0 "history")
string(REGEX MATCHALL "claude  (section.insert|item.compose)" records "${stdout}")
list(LENGTH records count)
if(NOT count EQUAL 3)
    message(FATAL_ERROR "the journal shows ${count} records of the actor claude, expected three:\n${stdout}")
endif()

# a refusal names its line or its param and applies nothing
file(WRITE "${WORK}/bad.txt" "section.insert item:${PIECE}@v5 after nope id c bars 4\n")
ursatz(--data "${DATA}" apply "${WORK}/bad.txt")
expect_status(2 "an insert after a section that is not there")
expect_text("unknown section 'nope' at /params/after" "an insert after a section that is not there")
file(WRITE "${WORK}/stale.txt" "section.delete item:${PIECE}@v4 id T0\n")
ursatz(--data "${DATA}" apply "${WORK}/stale.txt")
expect_status(2 "a delete against an old version")
expect_text("changed since you read it" "a delete against an old version")

message(STATUS "ok")
