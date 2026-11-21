# A listening round on the command line (P66 B7, R-674), run as processes on a fresh data folder: the none taste profile imported as text commands and a
# session opened, two batches of `listen --session`, `rate` with a pick, keep as default citing the pick, and the files they leave.
# Run with: cmake -DURSATZ=<ursatz> -DFIXTURES=<cli/tests/fixtures> -DWORK=<folder of its own> -P cli_listening_round.cmake

if(NOT URSATZ OR NOT FIXTURES OR NOT WORK)
    message(FATAL_ERROR "give -DURSATZ, -DFIXTURES and -DWORK")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(DATA "${WORK}/data")

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

function(expect_same first second what)
    file(READ "${first}" a HEX)
    file(READ "${second}" b HEX)
    if(NOT a STREQUAL b)
        message(FATAL_ERROR "${what}: ${first} and ${second} differ")
    endif()
endfunction()

# the none taste profile as text commands (its members as member lines), and a session
file(WRITE "${WORK}/setup.txt"
"settings.import scope taste:none note \"taste-profile.md none\"
  document
    motif
      durations half_beat 4 beat 2 two_beats 2 long_short 2
      features dotted 3 leap 0 repeat 0
      rung_moves step 6 skip 0
      decorations chord_tone 0 passing 4 neighbor 4 appoggiatura 2 escape 0
      avoid_repeats true
      leaps_per_motif 0
      accented_one_in 1
ratings.session what motifs taste none
")
ursatz(--data "${DATA}" apply "${WORK}/setup.txt")
expect_status(0 "apply of the profile and the session")
ursatz(--data "${DATA}" query ratings.sessions --format json)
expect_status(0 "query ratings.sessions")
string(JSON session GET "${stdout}" sessions 0 id)
if(NOT session MATCHES "^ses-")
    message(FATAL_ERROR "no session id in: ${stdout}")
endif()

# two batches of two motifs; B moves one setting
file(WRITE "${WORK}/b.json" "{\"motif\":{\"features\":{\"dotted\":1}}}")
ursatz(--data "${DATA}" listen --what motifs --count 2 --seed 1 --session "${session}" --round 1 --batch A --out-dir "${WORK}/out/A")
expect_status(0 "listen batch A")
ursatz(--data "${DATA}" listen --what motifs --count 2 --seed 1 --session "${session}" --round 1 --batch B --settings "${WORK}/b.json" --out-dir "${WORK}/out/B")
expect_status(0 "listen batch B")
ursatz(--data "${DATA}" listen --what motifs --count 2 --seed 1 --session "${session}" --round 1 --batch B --out-dir "${WORK}/out/B2")
if(status EQUAL 0 OR NOT out MATCHES "batch B of round 1 is recorded")
    message(FATAL_ERROR "a batch recorded twice was not refused (exit ${status}):\n${out}")
endif()

# the batch made in the session is the batch made without one, byte for byte
ursatz(listen --what motifs --count 2 --seed 1 --settings "${FIXTURES}/taste-none.json" --out-dir "${WORK}/out/plain")
expect_status(0 "listen with the profile as a settings file")
expect_same("${WORK}/out/A/01.mid" "${WORK}/out/plain/01.mid" "sample 1 of batch A")
expect_same("${WORK}/out/A/02.mid" "${WORK}/out/plain/02.mid" "sample 2 of batch A")

# rate: a rating of each batch and a pick
ursatz(--data "${DATA}" rate "${session}" --round 1 --seed 1 --rating A=3 --rating B=4 --better B --note test)
expect_status(0 "rate")
if(NOT stdout MATCHES "rat-[0-9a-f]+ A 3\n" OR NOT stdout MATCHES "rat-[0-9a-f]+ B 4\n" OR NOT stdout MATCHES "(pck-[0-9a-f]+) better B\n")
    message(FATAL_ERROR "rate printed:\n${stdout}")
endif()
string(REGEX MATCH "pck-[0-9a-f]+" pick "${stdout}")

# keep as default, citing the pick
file(WRITE "${WORK}/keep.txt" "ratings.keep taste none key motif.features.dotted value 1 ratings [${pick}] note test\n")
ursatz(--data "${DATA}" apply "${WORK}/keep.txt")
expect_status(0 "keep as default")

# the session file and the profile
file(READ "${DATA}/ratings/sessions/${session}.json" file)
string(JSON batches LENGTH "${file}" batches)
string(JSON ratings LENGTH "${file}" ratings)
string(JSON picks LENGTH "${file}" picks)
string(JSON first_samples LENGTH "${file}" batches 0 samples)
string(JSON second_samples LENGTH "${file}" batches 1 samples)
string(JSON sha GET "${file}" batches 0 samples 0 files 0 sha256)
if(NOT batches EQUAL 2 OR NOT first_samples EQUAL 2 OR NOT second_samples EQUAL 2 OR NOT ratings EQUAL 2 OR NOT picks EQUAL 1 OR NOT sha MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "the session file holds ${batches} batches (${first_samples}, ${second_samples} samples), ${ratings} ratings and ${picks} picks:\n${file}")
endif()
file(WRITE "${WORK}/scope.json" "{\"scope\":\"taste:none\"}")
ursatz(--data "${DATA}" query settings.scope --params "${WORK}/scope.json" --format json)
expect_status(0 "query settings.scope")
string(JSON dotted GET "${stdout}" values motif.features.dotted)
string(JSON kept LENGTH "${stdout}" kept)
if(NOT dotted EQUAL 1 OR NOT kept EQUAL 2)
    message(FATAL_ERROR "the profile holds dotted ${dotted} and ${kept} kept entries:\n${stdout}")
endif()

# refusals
ursatz(listen --what motifs --count 1 --round 1 --out-dir "${WORK}/out/refused")
expect_status(1 "listen --round without --session")
ursatz(--data "${DATA}" explore item:nothing)
if(status EQUAL 0 OR NOT out MATCHES "Error: ")
    message(FATAL_ERROR "explore of an item that is not there: exit ${status}:\n${out}")
endif()
ursatz(--data "${DATA}" rate "${session}" --round 1 --seed 1)
expect_status(1 "rate without a rating or a pick")
