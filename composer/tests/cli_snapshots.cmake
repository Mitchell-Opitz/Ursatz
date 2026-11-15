# Seeded CLI output compared with the committed snapshots in tests/snapshots (one file per output of a case).
# Run with: cmake -DCOMPOSER=<ursatz-composer> -DWORK=<empty working directory> -DSNAPSHOTS=<tests/snapshots>
#                 -DFIXTURES=<tests/fixtures> [-DUPDATE=1] -P cli_snapshots.cmake
# The values of the analyzer_version, composer_version and ursatz_commit keys are removed and the working and fixture
# directories are replaced by <WORK> and <FIXTURES> before comparing text; files ending .json are then compared as JSON
# values (member order and whitespace do not matter), MIDI, MusicXML and other text byte for byte.
# UPDATE=1 rewrites the snapshot files instead of comparing.

include("${CMAKE_CURRENT_LIST_DIR}/snapshot_json.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/normal")
file(MAKE_DIRECTORY "${SNAPSHOTS}")

set(failed "")

# Runs the composer in WORK and requires exit status 0.
function(composer)
    execute_process(COMMAND "${COMPOSER}" ${ARGN} WORKING_DIRECTORY "${WORK}"
                    OUTPUT_FILE "${WORK}/stdout.txt" ERROR_VARIABLE err RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "ursatz-composer ${ARGN}: exit ${status}\n${err}")
    endif()
endfunction()

# Compares (or, with UPDATE, writes) the snapshot <name> from the file <produced> in WORK.
function(snapshot name produced)
    if(NOT EXISTS "${WORK}/${produced}")
        message(FATAL_ERROR "case ${name}: the command wrote no ${produced}")
    endif()
    if(produced MATCHES "\\.(mid|musicxml)$")
        set(actual "${WORK}/${produced}")
    else()
        file(READ "${WORK}/${produced}" text)
        string(REGEX REPLACE "(\"(analyzer_version|composer_version|ursatz_commit)\" *: *)(\"[^\"]*\"|null)" "\\1\"\"" text "${text}")
        string(REPLACE "${WORK}" "<WORK>" text "${text}")
        string(REPLACE "${FIXTURES}" "<FIXTURES>" text "${text}")
        file(WRITE "${WORK}/normal/${name}" "${text}")
        set(actual "${WORK}/normal/${name}")
    endif()
    if(UPDATE)
        file(COPY_FILE "${actual}" "${SNAPSHOTS}/${name}")
        return()
    endif()
    if(NOT EXISTS "${SNAPSHOTS}/${name}")
        message(SEND_ERROR "missing snapshot ${name} (run with -DUPDATE=1 to create it)")
        return()
    endif()
    if(name MATCHES "\\.json$")
        snapshot_json_equal("${actual}" "${SNAPSHOTS}/${name}" same)
    else()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${actual}" "${SNAPSHOTS}/${name}" RESULT_VARIABLE status)
        if(status EQUAL 0)
            set(same TRUE)
        else()
            set(same FALSE)
        endif()
    endif()
    if(NOT same)
        message(SEND_ERROR "snapshot ${name} differs from the output of its command")
    endif()
endfunction()

# Runs a case: the arguments, then the snapshots <case>.stdout.<ext> and one per file the command writes.
function(run_case name stdout_ext)
    set(args ${ARGN})
    composer(${args})
    file(RENAME "${WORK}/stdout.txt" "${WORK}/${name}.stdout.${stdout_ext}")
    snapshot("${name}.stdout.${stdout_ext}" "${name}.stdout.${stdout_ext}")
    foreach(suffix plan.json mid musicxml)
        if(EXISTS "${WORK}/${name}.${suffix}")
            snapshot("${name}.${suffix}" "${name}.${suffix}")
        endif()
    endforeach()
endfunction()

# A request fixture (relative to the fixtures folder) with one placeholder replaced, written to WORK.
function(request fixture out placeholder value)
    file(READ "${FIXTURES}/${fixture}" text)
    string(REPLACE "${placeholder}" "${value}" text "${text}")
    file(WRITE "${WORK}/${out}" "${text}")
endfunction()

# --- theme ---------------------------------------------------------------------------

# chords.json and fast-minor.json carry a placeholder, filled as the tests do (a four-bar progression, the fast-minor style).
request(theme/chords.json theme-chords.request.json "@CHORDS@"
        "[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 1, \"text\": \"IV V@2\"}, {\"bar\": 2, \"text\": \"VI\"}, {\"bar\": 3, \"text\": \"V\"}]")
request(theme/fast-minor.json theme-fast-minor.request.json "@STYLE@" "${FIXTURES}/style-fast-minor.json")
set(theme_one-motive "${FIXTURES}/theme/one-motive.json")
set(theme_period-contrast "${FIXTURES}/theme/period-contrast.json")
set(theme_chords "${WORK}/theme-chords.request.json")
set(theme_fast-minor "${WORK}/theme-fast-minor.request.json")

foreach(case one-motive period-contrast chords fast-minor)
    run_case(theme-${case} json theme "${theme_${case}}" --seed 1 --format json
             --out theme-${case}.plan.json --midi theme-${case}.mid --musicxml theme-${case}.musicxml)
endforeach()
run_case(theme-one-motive-check txt theme "${theme_one-motive}" --seed 2 --check)

# --- piece ---------------------------------------------------------------------------

# The theme plan the theme-fill request names: the one-motive theme at seed 1.
composer(theme "${FIXTURES}/theme/one-motive.json" --seed 1 --out "${WORK}/source-theme.plan.json")
request(piece/theme-fill.json theme-fill.request.json "@THEME@" "${WORK}/source-theme.plan.json")
request(piece/loop-and-ending.json loop-and-ending.request.json "@STYLE@" "${FIXTURES}/style-fast-minor.json")

run_case(piece-theme-fill json piece "${WORK}/theme-fill.request.json" --seed 1 --format json
         --out piece-theme-fill.plan.json --midi piece-theme-fill.mid)
run_case(piece-loop-and-ending json piece "${WORK}/loop-and-ending.request.json" --seed 1 --format json
         --out piece-loop-and-ending.plan.json --midi piece-loop-and-ending.mid)
run_case(piece-progression-fill json piece "${FIXTURES}/piece/progression-fill.json" --seed 1 --format json
         --out piece-progression-fill.plan.json --midi piece-progression-fill.mid)
# The first section of theme-fill.json is "intro".
run_case(piece-regenerate json piece --from "${WORK}/piece-theme-fill.plan.json" --request "${WORK}/theme-fill.request.json"
         --regenerate intro --seed 2 --format json --out piece-regenerate.plan.json)

# --- plan, fill, lock, regenerate, variations and harmonize ---------------------------

run_case(plan-binary json plan --form binary --key C --meter 4/4 --seed 1 --format json --out plan-binary.plan.json)
run_case(plan-given json plan --key C --meter 4/4 --given-melody "${FIXTURES}/melody_c_major_8_bars.txt" --seed 1
         --format json --out plan-given.plan.json)
# The plans of the cases below are the plans above filled: seed 5, with the files and a tempo.
run_case(fill json fill plan-binary.plan.json --seed 5 --format json --out fill.plan.json --midi fill.mid
         --musicxml fill.musicxml --tempo 100)
run_case(fill-given json fill plan-given.plan.json --seed 5 --format json --out fill-given.plan.json)
run_case(lock json lock fill.plan.json --section A --format json --out lock.plan.json)
run_case(regenerate json regenerate fill.plan.json --section B --seed 3 --format json --out regenerate.plan.json
         --midi regenerate.mid --musicxml regenerate.musicxml)
run_case(variations json variations fill.plan.json --section B --count 2 --seed 3 --out-dir variations --format json --midi
         --musicxml)
run_case(harmonize json harmonize fill-given.plan.json --section B --count 2 --seed 3 --out-dir harmonizations --format json
         --midi --musicxml)
foreach(index 1 2)
    foreach(ext json mid musicxml)
        snapshot(variations.variation-${index}.${ext} variations/variation-${index}.${ext})
        snapshot(harmonize.variation-${index}.${ext} harmonizations/variation-${index}.${ext})
    endforeach()
endforeach()

# --- grow ----------------------------------------------------------------------------

foreach(into sentence period)
    run_case(grow-${into} json grow "${FIXTURES}/motive_d_minor.txt" --into ${into} --key "D minor" --meter 4/4 --seed 7
             --format json --out grow-${into}.plan.json --midi grow-${into}.mid)
endforeach()

# --- progression ---------------------------------------------------------------------

run_case(progression json progression --key "D minor" --meter 4/4 --bars 8 --cadence PAC --seed 7 --format json)

# --- counterpoint --------------------------------------------------------------------

foreach(species 1 3)
    run_case(counterpoint-species-${species} json counterpoint "${FIXTURES}/cantus_d_dorian.txt" --species ${species}
             --key D --mode minor --meter 4/4 --seed 1 --format json)
endforeach()

# --- motive commands -----------------------------------------------------------------

run_case(motive json motive "${FIXTURES}/motive_d_minor.json" --json)
run_case(variants-each-op json variants "${FIXTURES}/motive_d_minor.json" --each-op --seed 1 --json)
run_case(random-motive json random-motive --mode minor --meter 3/4 --strategy drawn --count 3 --seed 1 --json)

# --- GUI shapes ----------------------------------------------------------------------
# The calls the GUI backend makes (Ursatz-GUI routes/materials.c, service/theme_service.c, service/piece_service.c), with the
# arguments it builds and fixtures under tests/fixtures/gui written as the GUI writes its temp files. P26 moves the CLI onto the
# composer API; these snapshots must not move.

set(gui "${FIXTURES}/gui")

# Motive analysis of notes: meter, key, pickup, own harmony, a feature, a role and one tone override.
run_case(motive-notes json motive "${gui}/motive-notes.txt" --format notes --meter 4/4 --key "D minor" --pickup 0
         --harmony "I V@3" --feature a:0-2 --role melodic --tone 2=passing --json)
# Motive analysis of a rhythm.
run_case(motive-form-rhythm json motive "${gui}/motive-rhythm.txt" --form rhythm --meter 3/4 --pickup 1/4 --json)
# Degrees to notes.
run_case(motive-degrees json motive "${gui}/motive-degrees.txt" --format degrees --key "C minor" --meter 6/8 --pickup 0 --json)
# MIDI to motive.
run_case(motive-midi json motive "${FIXTURES}/motive_d_minor.mid" --format midi --meter 4/4 --key "C major" --json)
# Motive render in another key, with the MusicXML.
run_case(motive-render json motive "${gui}/motive-degrees.txt" --format degrees --form full --meter 4/4 --key "C major" --pickup 0
         --render-key "Eb major" --musicxml motive-render.musicxml --tempo 96 --json)

# The operation list.
run_case(variants-list-ops json variants --list-ops --json)
# Variants of a degree list against a basic motive, two functions, a distance, a count and a seed.
run_case(variants-basic json variants "${gui}/variants-source.txt" --format degrees --meter 4/4 --key "C major" --pickup 0
         --basic "${gui}/variants-basic.txt" --basic-format degrees --function beginning --function continuation
         --distance medium --count 4 --seed 3 --json)
# Recognize: the target is the source moved up one step of the scale.
run_case(recognize json recognize "${gui}/recognize-source.txt" "${gui}/recognize-target.txt" --format degrees
         --target-format degrees --meter 4/4 --key "C major" --pickup 0 --json)
# Random motives with a role (the style variant is covered by the API tests).
run_case(random-motive-style json random-motive --mode major --meter 4/4 --key "C major" --form full --role accompaniment
         --strategy drawn --count 2 --seed 5 --json)

# Random motives of the default strategy: segments, a harmony plan and the segment labels.
run_case(random-motive-segments json random-motive --mode minor --meter 4/4 --count 3 --seed 2 --json)

# The theme case the GUI runs (--format json --out --midi --musicxml) is the theme-* cases above, which write all three files.

# Piece regenerate of the theme-filled section, one lane (the plan from piece-theme-fill above).
run_case(piece-regenerate-lane json piece --from "${WORK}/piece-theme-fill.plan.json" --request "${WORK}/theme-fill.request.json"
         --regenerate a --lane bass --seed 2 --format json --out piece-regenerate-lane.plan.json)
# Piece variations of the same section: the printed result, result.json and each variation's plan, MIDI and MusicXML.
run_case(piece-variations json piece --from "${WORK}/piece-theme-fill.plan.json" --request "${WORK}/theme-fill.request.json"
         --variations a --lane harmony --count 2 --seed 2 --out-dir piece-variations --format json)
snapshot(piece-variations.result.json piece-variations/result.json)
foreach(index 1 2)
    foreach(ext json mid musicxml)
        snapshot(piece-variations.variation-${index}.${ext} piece-variations/variation-${index}.${ext})
    endforeach()
endforeach()

# --version: the version text is normalized to <VERSION> for this case only.
composer(--version)
file(READ "${WORK}/stdout.txt" version_text)
string(REGEX REPLACE "^ursatz-composer [^\n]*" "ursatz-composer <VERSION>" version_text "${version_text}")
file(WRITE "${WORK}/version.stdout.txt" "${version_text}")
snapshot(version.stdout.txt version.stdout.txt)

# --- refusals and exit statuses ------------------------------------------------------
# What the GUI reads back when a request fails: the message on stderr and the exit status (1 usage, 2 unsatisfiable, 3 write).

# Runs the composer, requires exit status <expected_status>, and snapshots stderr as <name>.stderr.txt and the status as <name>.status.txt.
function(run_error_case name expected_status)
    execute_process(COMMAND "${COMPOSER}" ${ARGN} WORKING_DIRECTORY "${WORK}"
                    OUTPUT_VARIABLE out ERROR_FILE "${WORK}/${name}.stderr.txt" RESULT_VARIABLE status)
    if(NOT status EQUAL expected_status)
        message(FATAL_ERROR "case ${name}: ursatz-composer ${ARGN}: exit ${status}, expected ${expected_status}")
    endif()
    file(WRITE "${WORK}/${name}.status.txt" "${status}\n")
    snapshot("${name}.stderr.txt" "${name}.stderr.txt")
    snapshot("${name}.status.txt" "${name}.status.txt")
endfunction()

# A bad meter to motive.
run_error_case(error-motive-meter 1 motive "${gui}/motive-notes.txt" --format notes --meter 5/3 --json)
# An unknown key in a theme request.
run_error_case(error-theme-key 1 theme "${gui}/theme-unknown-key.json" --seed 1)
# A request member of the wrong type in a piece request (beats as text).
run_error_case(error-piece-type 1 piece "${gui}/piece-wrong-type.json" --seed 1)
# A theme the Composer refuses: a last bar V under a perfect cadence.
run_error_case(error-theme-unsatisfiable 2 theme "${gui}/theme-unsatisfiable.json" --seed 1)
# An --out the Composer cannot write.
run_error_case(error-theme-write 3 theme "${theme_one-motive}" --seed 1 --out no-such-directory/theme.plan.json)

# A section that is not in the plan, and a plan that is not realized.
run_error_case(error-lock-section 2 lock fill.plan.json --section Q --out lock-refused.plan.json)
run_error_case(error-lock-open 2 lock plan-binary.plan.json --section A --out lock-refused.plan.json)

file(REMOVE_RECURSE "${WORK}/normal")
