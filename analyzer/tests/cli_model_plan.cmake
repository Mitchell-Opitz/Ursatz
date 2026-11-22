# ursatz analyze --model-plan (R-635), run as a process on a copy of Mozart 545: the plan file is written, validates against
# plan.v6.schema.json (when python3 with jsonschema was found), names the reading and the realization it comes from, and the option
# cannot be combined with another mode.
# Run with: cmake -DANALYZER=<ursatz> -DWORK=<working directory> -DMIDI=<Mozart 545.mid> -DSCHEMA=<plan.v6.schema.json>
#           -DFRAGMENT=<section.schema.json> [-DPYTHON=<python3> -DVALIDATOR=<validate_schema.py>] -P cli_model_plan.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY_FILE "${MIDI}" "${WORK}/mozart.mid")

execute_process(COMMAND "${ANALYZER}" analyze --model-plan "${WORK}/model-plan.json" "${WORK}/mozart.mid"
                WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "ursatz analyze --model-plan: exit ${status}\n${out}\n${err}")
endif()
if(NOT EXISTS "${WORK}/model-plan.json")
    message(FATAL_ERROR "the model plan was not written")
endif()
if(PYTHON)
    execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" --ref "${FRAGMENT}" "${SCHEMA}" "${WORK}/model-plan.json" RESULT_VARIABLE bad)
    if(NOT bad EQUAL 0)
        message(FATAL_ERROR "the model plan does not match plan.v6.schema.json")
    endif()
else()
    message(STATUS "schema not checked: no python3 with jsonschema")
endif()

file(READ "${WORK}/model-plan.json" plan)
string(JSON format GET "${plan}" format)
string(JSON reading GET "${plan}" derived_from reading)
string(JSON realization GET "${plan}" derived_from realization)
string(JSON producer GET "${plan}" metadata producer)
if(NOT format STREQUAL "ursatz-plan" OR NOT reading STREQUAL "ursatz-analyzer" OR NOT realization MATCHES "^[0-9a-f]+$"
   OR NOT producer STREQUAL "ursatz-analyzer")
    message(FATAL_ERROR "the model plan names: ${format}, ${reading}, ${realization}, ${producer}")
endif()
string(JSON sections LENGTH "${plan}" form children)
if(sections LESS 2)
    message(FATAL_ERROR "the model plan has ${sections} sections")
endif()

# Another command with the option is a usage error (exit 1), and nothing is written.
execute_process(COMMAND "${ANALYZER}" corpus "${WORK}" --model-plan "${WORK}/other.json"
                WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
if(NOT status EQUAL 1 OR EXISTS "${WORK}/other.json" OR NOT err MATCHES "unknown option")
    message(FATAL_ERROR "corpus with --model-plan: exit ${status}, expected 1 and an unknown option\n${err}")
endif()
message(STATUS "cli model plan: ok")
