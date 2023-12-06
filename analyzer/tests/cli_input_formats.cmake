# Checks the CLI's input format choice and its errors. Run with cmake -P and
# -DCLI=<ursatz-analyzer> -DMIDI_FIXTURE=<midi file> -DMUSICXML_DIR=<dir of
# MusicXML fixtures> -DWORK_DIR=<scratch dir>.

foreach(var CLI MIDI_FIXTURE MUSICXML_DIR WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

# check_case(<label> <source> <name in work dir> <expected exit code> <outputs expected> <cli args before the path>...)
# Copies the source to the work dir under the given name and runs the CLI on it.
function(check_case label source name expected_exit expect_outputs)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    configure_file("${source}" "${WORK_DIR}/${name}" COPYONLY)

    execute_process(COMMAND "${CLI}" ${ARGN} "${WORK_DIR}/${name}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}: ${error}")
    endif()

    file(GLOB outputs "${WORK_DIR}/*.report.md" "${WORK_DIR}/*.analysis.json")
    list(LENGTH outputs output_count)
    if(expect_outputs AND NOT output_count EQUAL 2)
        message(FATAL_ERROR "${label}: expected a report and an analysis document, found ${output_count} files")
    elseif(NOT expect_outputs AND NOT output_count EQUAL 0)
        message(FATAL_ERROR "${label}: expected no output files, found ${outputs}")
    endif()
    if(NOT expect_outputs AND error STREQUAL "")
        message(FATAL_ERROR "${label}: expected an error message on stderr")
    endif()
    message(STATUS "${label}: ok")
endfunction()

set(PICKUP "${MUSICXML_DIR}/pickup.musicxml")

check_case("musicxml by extension" "${PICKUP}" piece.musicxml 0 TRUE)
check_case("xml by extension" "${PICKUP}" piece.xml 0 TRUE)
check_case("musicxml by content" "${PICKUP}" piece.dat 0 TRUE)
check_case("midi by content" "${MIDI_FIXTURE}" piece.dat 0 TRUE)
check_case("override musicxml" "${PICKUP}" piece.mid 0 TRUE --input-format musicxml)
check_case("override midi" "${MIDI_FIXTURE}" piece.xml 0 TRUE --input-format midi)
check_case("midi read as musicxml" "${MIDI_FIXTURE}" piece.mid 1 FALSE --input-format musicxml)
check_case("musicxml read as midi" "${PICKUP}" piece.musicxml 1 FALSE --input-format midi)
check_case("bad format name" "${PICKUP}" piece.musicxml 1 FALSE --input-format abc)
check_case("format name missing" "${PICKUP}" piece.musicxml 1 FALSE --input-format)
check_case("malformed" "${MUSICXML_DIR}/malformed.musicxml" piece.musicxml 1 FALSE)
check_case("not a score" "${MUSICXML_DIR}/not-music.xml" piece.xml 1 FALSE)
check_case("score-timewise" "${MUSICXML_DIR}/timewise.musicxml" piece.musicxml 1 FALSE)
check_case("empty score" "${MUSICXML_DIR}/empty-score.musicxml" piece.musicxml 1 FALSE)
check_case("compressed" "${MUSICXML_DIR}/pickup.mxl" piece.mxl 1 FALSE)
check_case("unknown format" "${MUSICXML_DIR}/README.md" piece.txt 1 FALSE)
