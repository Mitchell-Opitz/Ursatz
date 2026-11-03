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
run_case(random-motive json random-motive --mode minor --meter 3/4 --count 3 --seed 1 --json)

file(REMOVE_RECURSE "${WORK}/normal")
