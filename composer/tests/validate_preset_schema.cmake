# Validates every built-in preset document (composer/presets/*.json) against schema/preset.schema.json, which references the preferences
# schema (schema/api/preferences.schema.json, passed with --ref). Run with cmake -P and -DPYTHON, -DVALIDATOR, -DSCHEMA, -DPREFERENCES and
# -DPRESETS_DIR. The test is registered only when python3 with jsonschema was found at configure time.

file(GLOB presets "${PRESETS_DIR}/*.json")
if(NOT presets)
    message(FATAL_ERROR "no preset documents in ${PRESETS_DIR}")
endif()

execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" --ref "${PREFERENCES}" "${SCHEMA}" ${presets} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "a preset document does not match the preset schema")
endif()
list(LENGTH presets count)
message(STATUS "${count} preset documents: valid")
