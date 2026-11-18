# Validates every built-in form document (composer/forms/*.json) against schema/form.v2.schema.json, which references the section
# fragment of the plan group (library/plan/schema/section.schema.json, passed with --ref). Run with cmake -P and -DPYTHON,
# -DVALIDATOR, -DSCHEMA, -DFRAGMENT and -DFORMS_DIR. The test is registered only when python3 with jsonschema was found at
# configure time (-DPYTHON is that python3).

file(GLOB forms "${FORMS_DIR}/*.json")
if(NOT forms)
    message(FATAL_ERROR "no form documents in ${FORMS_DIR}")
endif()

execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" --ref "${FRAGMENT}" "${SCHEMA}" ${forms} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "a form document does not match the form schema")
endif()
list(LENGTH forms count)
message(STATUS "${count} form documents: valid")
