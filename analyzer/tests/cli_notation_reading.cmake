# Checks the CLI's notation reading flags: what they do to the analysis document and which invocations they reject.
# Run with cmake -P and -DCLI=<ursatz-analyzer> -DFIXTURE=<a MusicXML file> -DPYTHON=<python3> -DHAVE_JSONSCHEMA=<bool>
# -DVALIDATOR=<validate_schema.py> -DSCHEMA=<schema file> -DWORK_DIR=<scratch dir>.

foreach(var CLI FIXTURE WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

set(INPUT "${WORK_DIR}/piece.musicxml")
set(DOCUMENT "${WORK_DIR}/piece.analysis.json")

# run_case(<label> <expected exit code> <document expected> <cli args before the path>...)
function(run_case label expected_exit expect_document)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    configure_file("${FIXTURE}" "${INPUT}" COPYONLY)

    execute_process(COMMAND "${CLI}" --json ${ARGN} "${INPUT}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
    if(NOT result EQUAL expected_exit)
        message(FATAL_ERROR "${label}: expected exit ${expected_exit}, got ${result}: ${error}")
    endif()
    if(expect_document AND NOT EXISTS "${DOCUMENT}")
        message(FATAL_ERROR "${label}: expected ${DOCUMENT} to be written")
    elseif(NOT expect_document AND EXISTS "${DOCUMENT}")
        message(FATAL_ERROR "${label}: expected no document")
    endif()
    message(STATUS "${label}: ok")
endfunction()

# json_value(<out> <json file text> <member>...)
function(json_value out text)
    string(JSON value GET "${text}" ${ARGN})
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# No flags: the meter is the score's and no reading is recorded.
run_case("no reading" 0 TRUE)
file(READ "${DOCUMENT}" plain)
json_value(source "${plain}" meter source)
string(JSON reading_type TYPE "${plain}" meter reading)
if(NOT source STREQUAL "notated" OR NOT reading_type STREQUAL "NULL")
    message(FATAL_ERROR "no reading: meter source '${source}', reading ${reading_type}")
endif()
json_value(plain_measures "${plain}" meter measure_count)

# A meter, scale and pickup: the document says so.
run_case("meter, scale and pickup" 0 TRUE --meter 3/4 --value-scale 2 --pickup 1/4)
file(READ "${DOCUMENT}" read)
json_value(source "${read}" meter source)
json_value(numerator "${read}" meter time_signature numerator)
json_value(denominator "${read}" meter time_signature denominator)
json_value(overridden "${read}" meter reading meter_overridden)
json_value(scale_num "${read}" meter reading scale num)
json_value(scale_den "${read}" meter reading scale den)
json_value(pickup_num "${read}" meter pickup length num)
json_value(pickup_den "${read}" meter pickup length den)
if(NOT source STREQUAL "override" OR NOT numerator EQUAL 3 OR NOT denominator EQUAL 4 OR NOT overridden
   OR NOT scale_num EQUAL 2 OR NOT scale_den EQUAL 1 OR NOT pickup_num EQUAL 1 OR NOT pickup_den EQUAL 4)
    message(FATAL_ERROR "reading: unexpected meter ${source} ${numerator}/${denominator} ${overridden} ${scale_num}/${scale_den} ${pickup_num}/${pickup_den}")
endif()
if(DEFINED PYTHON AND HAVE_JSONSCHEMA)
    execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" "${DOCUMENT}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "the document with a reading does not match the schema")
    endif()
endif()

# A scale alone leaves the meter the score's and says so.
run_case("scale only" 0 TRUE --value-scale 1/2)
file(READ "${DOCUMENT}" read)
json_value(source "${read}" meter source)
json_value(overridden "${read}" meter reading meter_overridden)
if(NOT source STREQUAL "notated" OR overridden)
    message(FATAL_ERROR "scale only: meter source '${source}', meter_overridden ${overridden}")
endif()

# Rejected: nothing is written.
run_case("pickup as long as the measure" 1 FALSE --meter 3/4 --pickup 3/4)
run_case("pickup longer than the measure" 1 FALSE --pickup 5/4)
run_case("scale not allowed" 1 FALSE --value-scale 3)
run_case("scale missing" 1 FALSE --value-scale)
run_case("meter malformed" 1 FALSE --meter 3-4)
run_case("meter beat type not a power of two" 1 FALSE --meter 3/3)
run_case("pickup zero" 1 FALSE --pickup 0/4)

# The flags belong to analyzing one file.
foreach(flag IN ITEMS --meter=3/4 --value-scale=2 --pickup=1/4)
    string(REPLACE "=" ";" flag_args "${flag}")
    execute_process(COMMAND "${CLI}" --corpus "${WORK_DIR}" ${flag_args} RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(NOT result EQUAL 1)
        message(FATAL_ERROR "corpus run with ${flag}: expected exit 1, got ${result}")
    endif()
endforeach()
message(STATUS "flags rejected outside a single-file analysis: ok")

file(REMOVE_RECURSE "${WORK_DIR}")
