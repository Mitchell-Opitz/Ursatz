# The variations and harmonize commands and lock --format json, run as processes (docs/cli-variations.md).
# Run with: cmake -DCOMPOSER=<ursatz-composer> -DWORK=<empty working directory> -DFIXTURES=<tests/fixtures> -P cli_variations.cmake

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

# --- the plans ---------------------------------------------------------------------

expect_status(0 plan --form rounded-binary --key C --meter 4/4 --bars 24 --seed 3 --out plan.json)
expect_status(0 fill plan.json --seed 3 --out filled.json)
expect_status(0 plan --form ternary --key C --meter 4/4 --given-melody "${FIXTURES}/melody_c_major_8_bars.txt" --at B
              --seed 1 --out given.json)
expect_status(0 fill given.json --seed 1 --out given-filled.json)

# --- variations: files and text --------------------------------------------------------

expect_status(0 variations filled.json --section B --count 3 --seed 3 --out-dir out --midi --musicxml)
foreach(i 1 2 3)
    expect_file(out/variation-${i}.json)
    expect_file(out/variation-${i}.mid)
    expect_file(out/variation-${i}.musicxml)
    # The Analyzer hears a variation as a form or, where it finds no theme, says so.
    expect_match("${OUT}" "variation ${i}  seed [0-9]+  B (heard as |no theme heard)")
endforeach()
expect_no_file(out/variation-4.json)
expect_match("${OUT}" "kept 3 of 3, [0-9]+ seeds tried\n$")

# Without --midi and --musicxml only the plans are written.
expect_status(0 variations filled.json --node 2 --lane harmony --count 2 --seed 5 --out-dir plain)
expect_file(plain/variation-1.json)
expect_file(plain/variation-2.json)
expect_no_file(plain/variation-1.mid)
expect_no_file(plain/variation-1.musicxml)

# A variation is a plan file the other commands read.
expect_status(0 export out/variation-1.json --midi again.mid)
expect_file(again.mid)

# --- variations: JSON --------------------------------------------------------------------

expect_status(0 variations filled.json --section B --count 2 --seed 3 --out-dir json --midi --format json)
string(JSON kept GET "${OUT}" kept)
string(JSON requested GET "${OUT}" requested)
string(JSON tried GET "${OUT}" seeds_tried)
string(JSON length LENGTH "${OUT}" variations)
if(NOT kept EQUAL 2 OR NOT requested EQUAL 2 OR NOT length EQUAL 2 OR tried LESS 2)
    message(FATAL_ERROR "bad counts in ${OUT}")
endif()
string(JSON index GET "${OUT}" variations 1 index)
string(JSON plan_path GET "${OUT}" variations 1 plan)
string(JSON midi_path GET "${OUT}" variations 1 midi)
string(JSON xml_type TYPE "${OUT}" variations 1 musicxml)
string(JSON seed_type TYPE "${OUT}" variations 1 seed)
string(JSON letter GET "${OUT}" variations 1 sections 0 letter)
string(JSON heard_type TYPE "${OUT}" variations 1 sections 0 cadences)
if(NOT index EQUAL 2 OR NOT plan_path STREQUAL "json/variation-2.json" OR NOT midi_path STREQUAL "json/variation-2.mid"
   OR NOT xml_type STREQUAL "NULL" OR NOT seed_type STREQUAL "NUMBER" OR NOT letter STREQUAL "B"
   OR NOT heard_type STREQUAL "ARRAY")
    message(FATAL_ERROR "bad variation member in ${OUT}")
endif()

# --- variations: the same seed gives the same files ---------------------------------------

expect_status(0 variations filled.json --section B --count 3 --seed 3 --out-dir repeat)
foreach(i 1 2 3)
    file(READ "${WORK}/out/variation-${i}.json" first)
    file(READ "${WORK}/repeat/variation-${i}.json" second)
    if(NOT first STREQUAL second)
        message(FATAL_ERROR "variation ${i} differs between runs of the same seed")
    endif()
endforeach()

# --- variations: exit 1 (usage) --------------------------------------------------------------

expect_status(1 variations filled.json --section B --out-dir no-count)
expect_status(1 variations filled.json --section B --count 0 --out-dir no-count)
expect_status(1 variations filled.json --section B --count 17 --out-dir no-count)
expect_status(1 variations filled.json --section B --count x --out-dir no-count)
expect_status(1 variations filled.json --section B --count 2)
expect_status(1 variations filled.json --count 2 --out-dir no-count)
expect_status(1 variations filled.json --node 2 --section B --count 2 --out-dir no-count)
expect_status(1 variations filled.json --section B --lane tenor --count 2 --out-dir no-count)
expect_status(1 variations filled.json --section B --count 2 --out-dir no-count --format xml)
expect_status(1 variations --section B --count 2 --out-dir no-count)
expect_no_file(no-count)

# --- variations: exit 2 (nothing written) --------------------------------------------------

expect_status(2 variations filled.json --section Z --count 2 --out-dir none)
expect_status(2 variations filled.json --node 99999 --count 2 --out-dir none)
expect_status(2 variations missing.json --section B --count 2 --out-dir none)
expect_no_file(none)
expect_status(0 lock filled.json --section A --out locked.json)
expect_status(2 variations locked.json --section A --count 2 --out-dir none)
expect_match("${ERR}" "Given")
expect_no_file(none)
expect_status(0 variations locked.json --section B --count 2 --seed 3 --out-dir after-lock)
expect_file(after-lock/variation-2.json)

# --- harmonize --------------------------------------------------------------------------

expect_status(0 harmonize given-filled.json --section B --count 3 --seed 1 --out-dir harm --midi --musicxml)
foreach(i 1 2 3)
    expect_file(harm/variation-${i}.json)
    expect_file(harm/variation-${i}.mid)
    expect_file(harm/variation-${i}.musicxml)
    # The Analyzer hears a variation as a form or, where it finds no theme, says so.
    expect_match("${OUT}" "variation ${i}  seed [0-9]+  B (heard as |no theme heard)")
endforeach()
expect_match("${OUT}" "kept 3 of 3, [0-9]+ seeds tried\n$")
expect_status(0 harmonize given-filled.json --section B --count 2 --seed 1 --out-dir harm-json --format json)
string(JSON kept GET "${OUT}" kept)
string(JSON midi_type TYPE "${OUT}" variations 0 midi)
if(NOT kept EQUAL 2 OR NOT midi_type STREQUAL "NULL")
    message(FATAL_ERROR "bad harmonize JSON ${OUT}")
endif()

# A section whose melody is not given, and the usage errors.
expect_status(2 harmonize given-filled.json --section A --count 2 --out-dir none)
expect_match("${ERR}" "no given melody")
expect_status(2 harmonize filled.json --section B --count 2 --out-dir none)
expect_no_file(none)
expect_status(1 harmonize given-filled.json --node 2 --count 2 --out-dir none)
expect_status(1 harmonize given-filled.json --section B --lane harmony --count 2 --out-dir none)
expect_status(1 harmonize given-filled.json --count 2 --out-dir none)
expect_status(1 harmonize given-filled.json --section B --count 0 --out-dir none)
expect_status(1 harmonize given-filled.json --section B --out-dir none)

# --- help -----------------------------------------------------------------------------------

expect_status(0 variations --help)
expect_match("${OUT}" "Usage:")
expect_status(0 harmonize --help)
expect_match("${OUT}" "Usage:")

# --- lock --format json ------------------------------------------------------------------------

expect_status(0 lock filled.json --section B --out locked-json.json --format json)
string(JSON nodes GET "${OUT}" locked nodes)
string(JSON spans GET "${OUT}" locked spans)
string(JSON lock_plan GET "${OUT}" plan)
if(nodes LESS 1 OR spans LESS 1 OR NOT lock_plan STREQUAL "locked-json.json")
    message(FATAL_ERROR "bad lock JSON ${OUT}")
endif()
expect_file(locked-json.json)
# Text output is unchanged.
expect_status(0 lock filled.json --section B --out locked-text.json)
expect_match("${OUT}" "^locked section B: [0-9]+ nodes and [0-9]+ lane spans now Given\nwrote locked-text.json\n$")
expect_status(0 lock filled.json --section B --out locked-text.json --format text)
expect_match("${OUT}" "^locked section B: ")
expect_status(1 lock filled.json --section B --out x.json --format xml)
expect_status(2 lock locked-json.json --section Z --out x.json --format json)
expect_no_file(x.json)
