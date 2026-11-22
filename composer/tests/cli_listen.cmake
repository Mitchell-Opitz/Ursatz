# The listen command, run as a process (cli/docs/cli-listen.md).
# Run with: cmake -DCOMPOSER=<ursatz> -DANALYZER=<ursatz> -DWORK=<empty working directory>
#           -DFIXTURES=<tests/fixtures> [-DSTYLE_FOLDER=ON] [-DCASE=overrides] -P cli_listen.cmake
# CASE=overrides runs only the overrides of the melody stage (R-672); CASE=motifs the motifs and settings sections, CASE=themes the
# themes from a generated motif, a motif file and triplets, and CASE=theme_settings the themes with settings, the style file and the
# usage errors (each also runs --print-settings and the refusals); without a case the script runs the rest.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Runs a program; sets OUT, ERR and STATUS in the caller.
function(run program)
    execute_process(COMMAND "${program}" ${ARGN} WORKING_DIRECTORY "${WORK}"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
    set(OUT "${out}" PARENT_SCOPE)
    set(ERR "${err}" PARENT_SCOPE)
    set(STATUS "${status}" PARENT_SCOPE)
endfunction()

function(expect_status expected)
    run("${COMPOSER}" ${ARGN})
    if(NOT STATUS EQUAL expected)
        message(FATAL_ERROR "ursatz ${ARGN}: exit ${STATUS}, expected ${expected}\n${OUT}\n${ERR}")
    endif()
    set(OUT "${OUT}" PARENT_SCOPE)
    set(ERR "${ERR}" PARENT_SCOPE)
endfunction()

function(expect_file path)
    if(NOT EXISTS "${WORK}/${path}")
        message(FATAL_ERROR "missing file ${path}")
    endif()
endfunction()

function(expect_match text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "expected /${pattern}/ in:\n${text}")
    endif()
endfunction()

# The files of samples 01 to <count> in a directory exist.
function(expect_samples dir count)
    foreach(i RANGE 1 ${count})
        set(name "0${i}")
        foreach(extension mid musicxml json)
            expect_file(${dir}/${name}.${extension})
        endforeach()
    endforeach()
    expect_file(${dir}/listen.jsonl)
    file(STRINGS "${WORK}/${dir}/listen.jsonl" lines)
    list(LENGTH lines lines_count)
    if(NOT lines_count EQUAL count)
        message(FATAL_ERROR "${dir}/listen.jsonl has ${lines_count} lines, expected ${count}")
    endif()
endfunction()

# Two files hold the same bytes (equal = TRUE) or different bytes (equal = FALSE).
function(compare_files equal first second)
    file(READ "${WORK}/${first}" a HEX)
    file(READ "${WORK}/${second}" b HEX)
    if(equal AND NOT a STREQUAL b)
        message(FATAL_ERROR "${first} and ${second} differ")
    elseif(NOT equal AND a STREQUAL b)
        message(FATAL_ERROR "${first} and ${second} are the same")
    endif()
endfunction()

# --- overrides (R-672) -------------------------------------------------------------------

# One theme at a seed with a settings document (or none): the file it wrote is <dir>/01.mid.
function(theme_with seed settings dir)
    set(arguments listen --what themes --count 1 --seed ${seed} --out-dir ${dir})
    if(NOT settings STREQUAL "")
        list(APPEND arguments --settings ${settings})
    endif()
    expect_status(0 ${arguments})
    expect_file(${dir}/01.mid)
endfunction()

if(CASE STREQUAL "overrides")
    # Waiving the parallel fifths and octaves changes the themes of seeds 4 and 14 (measured on the prototype, R-672) and not
    # seed 1.
    file(WRITE "${WORK}/waive.json" "{\"melody\": {\"waive\": [\"parallel-fifths\", \"parallel-octaves\"]}}")
    foreach(seed 1 4 14)
        theme_with(${seed} "" plain-${seed})
        theme_with(${seed} waive.json waive-${seed})
    endforeach()
    compare_files(TRUE plain-1/01.mid waive-1/01.mid)
    compare_files(FALSE plain-4/01.mid waive-4/01.mid)
    compare_files(FALSE plain-14/01.mid waive-14/01.mid)
    file(STRINGS "${WORK}/waive-4/listen.jsonl" lines)
    list(GET lines 0 first)
    string(JSON waived GET "${first}" preferences melody waive 0)
    if(NOT waived STREQUAL "parallel-fifths")
        message(FATAL_ERROR "the manifest does not record the waived rules: ${first}")
    endif()

    # basic_as_written (A20261115-80): the same themes without the development's own rules change at seed 1.
    file(WRITE "${WORK}/basic.json" "{\"development\": {\"basic_as_written\": true}}")
    theme_with(1 basic.json basic-1)
    compare_files(FALSE plain-1/01.mid basic-1/01.mid)

    # Reversing the leaps group changes the themes with a leaps weight of 5 at the seeds 1 to 4 (measured when the case was
    # written; seed 5 is the same, seed 6 differs).
    file(WRITE "${WORK}/leaps.json" "{\"melody\": {\"leaps\": 5}}")
    file(WRITE "${WORK}/leaps-reversed.json" "{\"melody\": {\"leaps\": 5, \"reverse\": [\"leaps\"]}}")
    foreach(seed 1 2 3 4)
        theme_with(${seed} leaps.json leaps-${seed})
        theme_with(${seed} leaps-reversed.json reversed-${seed})
        compare_files(FALSE leaps-${seed}/01.mid reversed-${seed}/01.mid)
    endforeach()

    # A name that is no rule is refused with the names.
    file(WRITE "${WORK}/bad.json" "{\"melody\": {\"waive\": [\"range\"]}}")
    run("${COMPOSER}" listen --what themes --count 1 --seed 1 --settings bad.json --out-dir bad)
    if(STATUS EQUAL 0)
        message(FATAL_ERROR "a waived rule that does not exist was accepted")
    endif()
    expect_match("${ERR}" "is not a rule the melody stage can waive")
    return()
endif()

if(NOT CASE STREQUAL "themes" AND NOT CASE STREQUAL "theme_settings")
# --- motifs ------------------------------------------------------------------------------

expect_status(0 listen --what motifs --count 3 --seed 5 --out-dir motifs-a)
expect_samples(motifs-a 3)
file(STRINGS "${WORK}/motifs-a/listen.jsonl" lines)
list(GET lines 1 second)
string(JSON seed GET "${second}" seed)
string(JSON index GET "${second}" index)
string(JSON status GET "${second}" status)
string(JSON what GET "${second}" what)
string(JSON motif GET "${second}" motif)
string(JSON settings TYPE "${second}" settings)
string(JSON preferences TYPE "${second}" preferences)
if(NOT seed EQUAL 6 OR NOT index EQUAL 2 OR NOT status STREQUAL "ok" OR NOT what STREQUAL "motifs"
   OR NOT motif STREQUAL "generated" OR NOT settings STREQUAL "NULL" OR NOT preferences STREQUAL "NULL")
    message(FATAL_ERROR "bad manifest line: ${second}")
endif()
string(JSON file_count LENGTH "${second}" files)
if(NOT file_count EQUAL 3)
    message(FATAL_ERROR "bad files in the manifest line: ${second}")
endif()

# The same options and seed give the same files in another directory.
expect_status(0 listen --what motifs --count 3 --seed 5 --out-dir motifs-b)
foreach(i 01 02 03)
    foreach(extension mid musicxml json)
        compare_files(TRUE motifs-a/${i}.${extension} motifs-b/${i}.${extension})
    endforeach()
endforeach()

# --- settings ----------------------------------------------------------------------------

file(WRITE "${WORK}/settings.json" "{\"motif\": {\"bars\": {\"one\": 0, \"two\": 1}}}")
expect_status(0 listen --what motifs --count 3 --seed 5 --settings settings.json --out-dir motifs-s)
expect_samples(motifs-s 3)
set(changed FALSE)
foreach(i 01 02 03)
    file(READ "${WORK}/motifs-a/${i}.mid" before HEX)
    file(READ "${WORK}/motifs-s/${i}.mid" after HEX)
    if(NOT before STREQUAL after)
        set(changed TRUE)
    endif()
endforeach()
if(NOT changed)
    message(FATAL_ERROR "the settings changed no motif")
endif()
file(STRINGS "${WORK}/motifs-s/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON settings GET "${first}" settings)
string(JSON bars_one GET "${first}" preferences motif bars one)
if(NOT settings STREQUAL "settings.json" OR NOT bars_one EQUAL 0)
    message(FATAL_ERROR "the manifest does not record the settings: ${first}")
endif()

# All nine listening settings on (R-422): the command runs and the manifest records the document.
file(WRITE "${WORK}/listening.json"
     "{\"motif\": {\"accented_one_in\": 2, \"avoid_repeats\": true, \"leaps_per_motif\": 2, \"leap_recovery\": true, "
     "\"pickup_from_accent\": true, \"contrast_fidelity_min\": \"1/3\", \"long_on_weak\": 4, \"final_long\": true}}")
expect_status(0 listen --what motifs --count 3 --seed 5 --settings listening.json --out-dir motifs-l)
expect_samples(motifs-l 3)
file(STRINGS "${WORK}/motifs-l/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON listening_one_in GET "${first}" preferences motif accented_one_in)
string(JSON listening_final GET "${first}" preferences motif final_long)
string(JSON listening_ratio GET "${first}" preferences motif contrast_fidelity_min)
if(NOT listening_one_in EQUAL 2 OR NOT listening_final STREQUAL "ON" OR NOT listening_ratio STREQUAL "1/3")
    message(FATAL_ERROR "the manifest does not record the listening settings: ${first}")
endif()

# Every L2 setting on (R-440 to R-444): the command runs in 4/4 and in 6/8, and the manifest records the document.
file(WRITE "${WORK}/l2.json"
     "{\"motif\": {\"durations\": {\"half_beat\": 1, \"beat\": 1, \"two_beats\": 1, \"quarter_beats\": 1, "
     "\"long_short\": 1, \"short_long\": 1, \"triplet\": 1, \"triplet_two_beats\": 1}, \"compound_beats\": true, "
     "\"endings\": {\"free\": 0, \"closed\": 2, \"open\": 1, \"lead_in\": 1}, \"long_nct_one_in\": 1}}")
expect_status(0 listen --what motifs --count 3 --seed 5 --settings l2.json --out-dir motifs-l2)
expect_samples(motifs-l2 3)
expect_status(0 listen --what motifs --count 3 --seed 5 --meter 6/8 --settings l2.json --out-dir motifs-l2-68)
expect_samples(motifs-l2-68 3)
file(STRINGS "${WORK}/motifs-l2/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON l2_one_in GET "${first}" preferences motif long_nct_one_in)
string(JSON l2_compound GET "${first}" preferences motif compound_beats)
string(JSON l2_closed GET "${first}" preferences motif endings closed)
string(JSON l2_triplet GET "${first}" preferences motif durations triplet)
if(NOT l2_one_in EQUAL 1 OR NOT l2_compound STREQUAL "ON" OR NOT l2_closed EQUAL 2 OR NOT l2_triplet EQUAL 1)
    message(FATAL_ERROR "the manifest does not record the L2 settings: ${first}")
endif()

# Every L3 motif setting on (R-453): the command runs and the manifest records it; a plan that does not fit the meter is
# recorded as refused.
file(WRITE "${WORK}/l3.json"
     "{\"motif\": {\"syncopation_one_in\": 1, \"skips_per_motif\": 2, \"plan\": [\"ii\", \"V\"]}}")
expect_status(0 listen --what motifs --count 3 --seed 5 --settings l3.json --out-dir motifs-l3)
expect_samples(motifs-l3 3)
file(STRINGS "${WORK}/motifs-l3/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON l3_sync GET "${first}" preferences motif syncopation_one_in)
string(JSON l3_skips GET "${first}" preferences motif skips_per_motif)
string(JSON l3_plan GET "${first}" preferences motif plan 0)
if(NOT l3_sync EQUAL 1 OR NOT l3_skips EQUAL 2 OR NOT l3_plan STREQUAL "ii")
    message(FATAL_ERROR "the manifest does not record the L3 settings: ${first}")
endif()
file(WRITE "${WORK}/l3-refused.json" "{\"motif\": {\"plan\": [\"I\", \"vi\", \"ii\"]}}")
expect_status(2 listen --what motifs --count 3 --seed 5 --meter 3/4 --settings l3-refused.json --out-dir motifs-l3-refused)
file(STRINGS "${WORK}/motifs-l3-refused/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON l3_status GET "${first}" status)
string(FIND "${l3_status}" "refused: /preferences/motif/plan" l3_found)
if(NOT l3_found EQUAL 0)
    message(FATAL_ERROR "a plan that does not fit 3/4 is not recorded as refused: ${first}")
endif()
endif()

if(NOT CASE STREQUAL "motifs" AND NOT CASE STREQUAL "theme_settings")
# --- themes ------------------------------------------------------------------------------

# A generated motif for each sample.
expect_status(0 listen --what themes --count 2 --seed 1 --out-dir themes-generated)
expect_samples(themes-generated 2)

# A motif file, in 6/8.
expect_status(0 listen --what themes --count 1 --seed 1 --meter 6/8 --motif "${FIXTURES}/motifs/test-motif-6-8.json"
              --out-dir themes-motif)
expect_samples(themes-motif 1)
file(STRINGS "${WORK}/themes-motif/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON motif GET "${first}" motif)
if(NOT motif STREQUAL "${FIXTURES}/motifs/test-motif-6-8.json")
    message(FATAL_ERROR "the manifest does not name the motif file: ${first}")
endif()

# A motif file of eighth triplets (R-442): its notes text writes with triplet values, the theme request is built from it, and
# both samples are composed.
file(WRITE "${WORK}/triplet-motif.txt" "C4:e3 D4:e3 E4:e3 G4:q E4:e3 D4:e3 C4:e3 E4:q G4:h C5:h\n")
expect_status(0 listen --what themes --count 2 --seed 1 --meter 4/4 --motif triplet-motif.txt --out-dir themes-triplets)
expect_samples(themes-triplets 2)
file(STRINGS "${WORK}/themes-triplets/listen.jsonl" lines)
foreach(line IN LISTS lines)
    string(JSON status GET "${line}" status)
    if(NOT status STREQUAL "ok")
        message(FATAL_ERROR "a theme from the eighth-triplet motif is not ok: ${line}")
    endif()
endforeach()

endif()

if(NOT CASE STREQUAL "motifs" AND NOT CASE STREQUAL "themes")
# --- theme settings ----------------------------------------------------------------------

# Every development setting on (R-453): the continuation and the basic idea as written reach the grow run (B4's fixture).
file(WRITE "${WORK}/l3-development.json"
     "{\"development\": {\"continuation\": {\"every_segment\": true, \"fragment_lengths\": {\"segment\": 1, \"half_bar\": 1, "
     "\"beat\": 1}, \"diminish\": true, \"liquidate\": true, \"faster_harmony\": true}, \"basic_as_written\": true}}")
expect_status(0 listen --what themes --count 2 --seed 1 --motif "${FIXTURES}/continuation/s77.json"
              --settings l3-development.json --out-dir themes-l3)
expect_samples(themes-l3 2)

# Every L4 setting on (R-474): the closure, the cap on bars with one rhythm, leaps, register, the contrast distance and new
# material reach the grow run, and the command accepts the settings file.
file(WRITE "${WORK}/l4-settings.json"
     "{\"melody\": {\"closure\": {\"final\": 20, \"first\": 10}, \"rhythm_repeat_bars\": 3, \"leaps\": 10, \"register\": 10}, "
     "\"development\": {\"contrast\": {\"distance\": {\"similar\": 1, \"distant\": 1}, \"new_material\": 1}}}")
expect_status(0 listen --what themes --count 2 --seed 1 --into period --bars 8 --settings l4-settings.json --out-dir themes-l4)
file(STRINGS "${WORK}/themes-l4/listen.jsonl" lines)
list(GET lines 0 first)
string(JSON l4_final GET "${first}" preferences melody closure final)
string(JSON l4_repeat GET "${first}" preferences melody rhythm_repeat_bars)
string(JSON l4_new GET "${first}" preferences development contrast new_material)
if(NOT l4_final EQUAL 20 OR NOT l4_repeat EQUAL 3 OR NOT l4_new EQUAL 1)
    message(FATAL_ERROR "the manifest does not record the L4 settings: ${first}")
endif()

# A theme request: the same MIDI as the theme command writes for the seed.
expect_status(0 listen --what themes --count 1 --seed 2 --theme-request "${FIXTURES}/theme/one-motive.json" --out-dir themes-request)
expect_samples(themes-request 1)
expect_status(0 theme "${FIXTURES}/theme/one-motive.json" --seed 2 --midi theme.mid)
compare_files(TRUE themes-request/01.mid theme.mid)

# Settings reach a theme (seed 2 of one-motive.json: attempts 1, keep 1 and against_bass 20 change the heard text).
file(WRITE "${WORK}/theme-settings.json" "{\"melody\": {\"attempts\": 1, \"keep\": 1, \"weights\": {\"against_bass\": 20}}}")
expect_status(0 listen --what themes --count 1 --seed 2 --theme-request "${FIXTURES}/theme/one-motive.json"
              --settings theme-settings.json --out-dir themes-settings)
compare_files(FALSE themes-request/01.mid themes-settings/01.mid)

# A sample that cannot be composed is a manifest line with the reason and no files; the command goes on and exits 2 when
# none was written (a 4-bar period with the analysis check gets stuck at these seeds).
expect_status(2 listen --what themes --count 2 --seed 1 --into period --bars 4 --development theme --check --out-dir themes-stuck)
file(STRINGS "${WORK}/themes-stuck/listen.jsonl" lines)
list(LENGTH lines lines_count)
if(NOT lines_count EQUAL 2)
    message(FATAL_ERROR "themes-stuck has ${lines_count} manifest lines, expected 2")
endif()
list(GET lines 1 second)
string(JSON status GET "${second}" status)
string(JSON file_count LENGTH "${second}" files)
if(NOT status MATCHES "^stuck: " OR NOT file_count EQUAL 0)
    message(FATAL_ERROR "bad line for a sample that could not be composed: ${second}")
endif()
if(EXISTS "${WORK}/themes-stuck/01.mid")
    message(FATAL_ERROR "a sample that could not be composed wrote files")
endif()

# --- a style ---------------------------------------------------------------------------------

# Smoke: a style model file of the fixtures.
expect_status(0 listen --what themes --count 1 --seed 1 --mode minor --style "${FIXTURES}/style-fast-minor.json" --out-dir themes-style-file)
expect_samples(themes-style-file 1)
expect_status(0 listen --what motifs --count 1 --seed 1 --mode minor --style "${FIXTURES}/style-fast-minor.json" --out-dir motifs-style-file)
expect_samples(motifs-style-file 1)

# A style folder made into a model by the Analyzer, end to end (slow: the corpus is analyzed; STYLE_FOLDER is ON).
if(STYLE_FOLDER)
    run("${ANALYZER}" corpus "${FIXTURES}/corpus-sonatinas" --out analyzed)
    if(NOT STATUS EQUAL 0)
        message(FATAL_ERROR "ursatz corpus: exit ${STATUS}\n${OUT}\n${ERR}")
    endif()
    run("${ANALYZER}" style-profile analyzed --name sonatinas --out sonatinas.style.json)
    if(NOT STATUS EQUAL 0)
        message(FATAL_ERROR "ursatz style-profile: exit ${STATUS}\n${OUT}\n${ERR}")
    endif()
    expect_status(0 listen --what themes --count 1 --seed 1 --style sonatinas.style.json --out-dir themes-style)
    expect_samples(themes-style 1)
    file(STRINGS "${WORK}/themes-style/listen.jsonl" lines)
    list(GET lines 0 first)
    string(JSON style GET "${first}" style)
    if(NOT style STREQUAL "sonatinas.style.json")
        message(FATAL_ERROR "the manifest does not name the style: ${first}")
    endif()
endif()
endif()

# --- --print-settings and exit 1 -----------------------------------------------------------

expect_status(0 listen --print-settings)
string(JSON keep GET "${OUT}" melody keep)
string(JSON bars_one GET "${OUT}" motif bars one)
string(JSON fidelity TYPE "${OUT}" development fidelity_minimum)
if(NOT keep EQUAL 6 OR NOT bars_one EQUAL 1 OR NOT fidelity STREQUAL "OBJECT")
    message(FATAL_ERROR "bad default settings: ${OUT}")
endif()

# The defaults document is a valid settings file and changes nothing.
file(WRITE "${WORK}/defaults.json" "${OUT}")
if(NOT CASE STREQUAL "themes" AND NOT CASE STREQUAL "theme_settings")
    expect_status(0 listen --what motifs --count 3 --seed 5 --settings defaults.json --out-dir motifs-defaults)
    foreach(i 01 02 03)
        compare_files(TRUE motifs-a/${i}.mid motifs-defaults/${i}.mid)
    endforeach()
endif()

file(WRITE "${WORK}/bad-settings.json" "{\"melody\": {\"keep\": 99}}")
expect_status(1 listen --what motifs --settings bad-settings.json --out-dir bad-dir)
expect_match("${ERR}" "/melody/keep")
expect_status(1 listen --what themes --motif "${FIXTURES}/motifs/test-motif-6-8.json"
              --theme-request "${FIXTURES}/theme/one-motive.json" --out-dir bad-dir)
expect_status(1 listen --what motifs --out-dir bad-dir --settings missing.json)
expect_status(1 listen --what chords --out-dir bad-dir)
expect_status(1 listen --what motifs)
