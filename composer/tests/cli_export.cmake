# The --tempo and --plain-midi options of the exporting commands, run as processes (docs/export.md).
# Run with: cmake -DCOMPOSER=<ursatz-composer> -DWORK=<empty working directory> -P cli_export.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Runs the composer; sets OUT, ERR and STATUS in the caller.
function(composer)
    execute_process(COMMAND "${COMPOSER}" ${ARGN} WORKING_DIRECTORY "${WORK}"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
    set(OUT "${out}" PARENT_SCOPE)
    set(ERR "${err}" PARENT_SCOPE)
    set(STATUS "${status}" PARENT_SCOPE)
endfunction()

function(expect_status expected)
    composer(${ARGN})
    if(NOT STATUS EQUAL expected)
        message(FATAL_ERROR "ursatz-composer ${ARGN}: exit ${STATUS}, expected ${expected}\n${OUT}\n${ERR}")
    endif()
    set(OUT "${OUT}" PARENT_SCOPE)
    set(ERR "${ERR}" PARENT_SCOPE)
endfunction()

function(expect_file path)
    if(NOT EXISTS "${WORK}/${path}")
        message(FATAL_ERROR "missing file ${path}")
    endif()
endfunction()

function(expect_no_file path)
    if(EXISTS "${WORK}/${path}")
        message(FATAL_ERROR "unexpected file ${path}")
    endif()
endfunction()

function(expect_match text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "expected /${pattern}/ in:\n${text}")
    endif()
endfunction()

# The set-tempo meta-event (FF 51 03 + 3 bytes) of a file as hex, or "" when it has none.
function(tempo_of file out)
    file(READ "${WORK}/${file}" hex HEX)
    if(hex MATCHES "ff5103([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f])")
        set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

# --- a filled plan -------------------------------------------------------------------

expect_status(0 plan --form rounded-binary --key C --meter 4/4 --bars 24 --seed 3 --out plan.json)
expect_status(0 fill plan.json --seed 3 --out filled.json --midi default.mid --musicxml default.musicxml)
tempo_of(default.mid default_tempo)
if(NOT default_tempo STREQUAL "")
    message(FATAL_ERROR "a file written with no --tempo has a tempo: ${default_tempo}")
endif()
file(READ "${WORK}/default.musicxml" default_xml)
if(default_xml MATCHES "<sound tempo=")
    message(FATAL_ERROR "MusicXML with no --tempo has a tempo")
endif()

# --- --tempo on every exporting command ------------------------------------------------
# A tempo is written as whole microseconds per quarter note, so MusicXML shows 60000000 / that: whole for 96, 150, 60.

# 96 quarter notes per minute is 625000 microseconds per quarter note, 0x098968.
expect_status(0 fill plan.json --seed 3 --out tempo.json --midi tempo.mid --musicxml tempo.musicxml --tempo 96)
tempo_of(tempo.mid fill_tempo)
expect_match("${fill_tempo}" "^098968$")
file(READ "${WORK}/tempo.musicxml" tempo_xml)
expect_match("${tempo_xml}" "<sound tempo=\"96\"/>")

expect_status(0 export filled.json --midi export.mid --musicxml export.musicxml --tempo 96)
tempo_of(export.mid export_tempo)
expect_match("${export_tempo}" "^098968$")
file(READ "${WORK}/export.musicxml" export_xml)
expect_match("${export_xml}" "<sound tempo=\"96\"/>")

# A fraction: 195/2 = 97.5 quarter notes per minute, 615385 microseconds (0x0963d9).
expect_status(0 export filled.json --midi fraction.mid --tempo 195/2)
tempo_of(fraction.mid fraction_tempo)
expect_match("${fraction_tempo}" "^0963d9$")

expect_status(0 regenerate filled.json --section B --seed 5 --out regenerated.json --midi regenerated.mid --tempo 60)
tempo_of(regenerated.mid regenerate_tempo)
expect_match("${regenerate_tempo}" "^0f4240$")

expect_status(0 variations filled.json --section B --count 2 --seed 3 --out-dir variations --midi --musicxml --tempo 150)
expect_file(variations/variation-1.mid)
tempo_of(variations/variation-1.mid variation_tempo)
expect_match("${variation_tempo}" "^061a80$")
file(READ "${WORK}/variations/variation-1.musicxml" variation_xml)
expect_match("${variation_xml}" "<sound tempo=\"150\"/>")

# --- --plain-midi ------------------------------------------------------------------------

expect_status(0 fill plan.json --seed 3 --out plain.json --midi plain.mid --plain-midi)
file(SIZE "${WORK}/plain.mid" plain_size)
file(SIZE "${WORK}/default.mid" default_size)
if(NOT plain_size LESS default_size)
    message(FATAL_ERROR "the plain MIDI file (${plain_size} bytes) is not smaller than the named one (${default_size})")
endif()
file(READ "${WORK}/plain.mid" plain_hex HEX)
file(READ "${WORK}/default.mid" default_hex HEX)
expect_match("${default_hex}" "ff03")
expect_match("${default_hex}" "ff06")
# The plain file has no track name and no marker meta-event at all.
string(REGEX MATCHALL "ff0[36]" plain_meta "${plain_hex}")
list(LENGTH plain_meta plain_meta_count)
if(NOT plain_meta_count EQUAL 0)
    message(FATAL_ERROR "the plain MIDI file has track name or marker events")
endif()

expect_status(0 export filled.json --midi plain-export.mid --plain-midi)
file(SIZE "${WORK}/plain-export.mid" plain_export_size)
if(NOT plain_export_size EQUAL plain_size)
    message(FATAL_ERROR "export --plain-midi and fill --plain-midi differ in size")
endif()

# --- usage errors -------------------------------------------------------------------------

foreach(bad 19 401 0 abc 96/0 -5)
    composer(export filled.json --midi bad.mid --tempo "${bad}")
    if(NOT STATUS EQUAL 1)
        message(FATAL_ERROR "export --tempo '${bad}': exit ${STATUS}, expected 1\n${OUT}\n${ERR}")
    endif()
    expect_match("${ERR}" "--tempo must be from 20 to 400")
    expect_no_file(bad.mid)
endforeach()
expect_status(1 fill plan.json --seed 3 --out bad.json --tempo 500)
expect_no_file(bad.json)
expect_status(1 regenerate filled.json --section B --out bad.json --tempo fast)
expect_no_file(bad.json)
expect_status(1 variations filled.json --section B --count 2 --out-dir bad --midi --tempo 10)
expect_no_file(bad)
expect_status(1 export filled.json --midi bad.mid --tempo 96 --tempo 97)
expect_no_file(bad.mid)

# 20 and 400 are the limits.
expect_status(0 export filled.json --midi low.mid --tempo 20)
expect_status(0 export filled.json --midi high.mid --tempo 400)

# --tempo needs a value.
expect_status(1 export filled.json --midi bad.mid --tempo)
expect_no_file(bad.mid)
