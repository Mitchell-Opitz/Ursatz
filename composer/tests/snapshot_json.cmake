# Compares two JSON files as values: object member order and whitespace do not matter, array order does.
# Sets <result> to TRUE when both files hold equal JSON values, FALSE when they differ or either is not valid JSON.
function(snapshot_json_equal actual expected result)
    file(READ "${actual}" actual_text)
    file(READ "${expected}" expected_text)
    string(JSON same ERROR_VARIABLE err EQUAL "${actual_text}" "${expected_text}")
    if(NOT err STREQUAL "NOTFOUND" OR NOT same)
        set(${result} FALSE PARENT_SCOPE)
    else()
        set(${result} TRUE PARENT_SCOPE)
    endif()
endfunction()
