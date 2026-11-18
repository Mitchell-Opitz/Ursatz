# Validates the section documents test_plan_section wrote (plan_section_written_<use>_<n>.json) against the profile of their use in
# library/plan/schema/section.schema.json. A profile is a definition of the fragment, so each use gets a one-line schema that
# references it. Run with cmake -P and -DPYTHON, -DVALIDATOR, -DFRAGMENT, -DWRITTEN_DIR and -DWORK_DIR.

file(MAKE_DIRECTORY "${WORK_DIR}")
file(READ "${FRAGMENT}" fragment_text)
string(JSON fragment_id GET "${fragment_text}" "$id")

set(profile_form form_section)
set(profile_plan plan_section)
set(profile_request request_section)
set(profile_overlay overlay)

foreach(use form plan request overlay)
    file(GLOB documents "${WRITTEN_DIR}/plan_section_written_${use}_*.json")
    if(NOT documents)
        message(FATAL_ERROR "no ${use} section documents in ${WRITTEN_DIR}")
    endif()
    set(schema "${WORK_DIR}/${use}.schema.json")
    file(WRITE "${schema}" "{\"$schema\": \"https://json-schema.org/draft/2020-12/schema\", \"$ref\": \"${fragment_id}#/$defs/${profile_${use}}\"}\n")
    execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" --ref "${FRAGMENT}" "${schema}" ${documents} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "a ${use} section document does not match ${profile_${use}}")
    endif()
    list(LENGTH documents count)
    message(STATUS "${count} ${use} documents: valid")
endforeach()
