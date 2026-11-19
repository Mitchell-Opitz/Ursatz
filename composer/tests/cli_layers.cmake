# --layers of theme, piece and fill, run as processes (R-634): the three files of the layers are written, they validate against their
# schemas when python3 with jsonschema was found, the reading names the realization by its digest, no layers are written without the
# option, and the result printed does not carry them.
# Run with: cmake -DCOMPOSER=<ursatz-composer> -DWORK=<empty working directory> -DFIXTURES=<tests/fixtures>
#           -DREALIZATION_SCHEMA=<file> -DREADING_SCHEMA=<file> [-DPYTHON=<python3> -DVALIDATOR=<validate_schema.py>] -P cli_layers.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

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
endfunction()

# The three files of a layers folder exist, validate, and the reading's target digest is the realization's digest.
function(expect_layers dir)
    foreach(file plan.json realization.json readings/composer.json)
        if(NOT EXISTS "${WORK}/${dir}/${file}")
            message(FATAL_ERROR "missing ${dir}/${file}")
        endif()
    endforeach()
    if(PYTHON)
        execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${REALIZATION_SCHEMA}" "${WORK}/${dir}/realization.json" RESULT_VARIABLE bad)
        if(NOT bad EQUAL 0)
            message(FATAL_ERROR "${dir}/realization.json does not match its schema")
        endif()
        execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${READING_SCHEMA}" "${WORK}/${dir}/readings/composer.json" RESULT_VARIABLE bad)
        if(NOT bad EQUAL 0)
            message(FATAL_ERROR "${dir}/readings/composer.json does not match its schema")
        endif()
    endif()
    file(READ "${WORK}/${dir}/readings/composer.json" reading)
    string(JSON digest GET "${reading}" target digest)
    if(NOT digest MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${dir}: the reading names no realization version")
    endif()
    file(READ "${WORK}/${dir}/plan.json" plan)
    string(JSON format GET "${plan}" format)
endfunction()

# theme: the layers of the first theme, and none without the option; the printed result carries no layers.
expect_status(0 theme "${FIXTURES}/theme/one-motive.json" --seed 1 --layers layers-theme --format json)
expect_layers(layers-theme)
if(OUT MATCHES "\"realization\"" OR OUT MATCHES "\"reading\"")
    message(FATAL_ERROR "theme printed the layers: ${OUT}")
endif()
expect_status(0 theme "${FIXTURES}/theme/one-motive.json" --seed 1 --out plain.json)
if(EXISTS "${WORK}/readings" OR EXISTS "${WORK}/realization.json")
    message(FATAL_ERROR "layers written without --layers")
endif()

# fill: a plan from theme's --out, realized, with its layers.
expect_status(0 fill plain.json --seed 1 --layers layers-fill)
expect_layers(layers-fill)

# piece: the layers of the first version (the loop).
expect_status(0 piece "${FIXTURES}/piece/rounded-binary.json" --seed 1 --layers layers-piece)
expect_layers(layers-piece)

# A folder that cannot be made is a write failure (exit 3), and the message names it.
file(WRITE "${WORK}/blocked" "a file, not a folder")
composer(theme "${FIXTURES}/theme/one-motive.json" --seed 1 --layers "${WORK}/blocked/layers")
if(NOT STATUS EQUAL 3)
    message(FATAL_ERROR "an unwritable --layers folder gave exit ${STATUS}, expected 3\n${ERR}")
endif()
if(NOT ERR MATCHES "cannot write")
    message(FATAL_ERROR "no 'cannot write' in: ${ERR}")
endif()
message(STATUS "cli layers: ok")
