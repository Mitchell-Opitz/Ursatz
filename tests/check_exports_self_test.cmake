# Plants a wrong export file and checks that check_exports.cmake refuses it (a check that never fails would pass the real libraries too).
#   cmake -DNM=<nm> -DLIBRARY=<exports_self_test archive> -DSCRIPT=<check_exports.cmake> -DWORK=<folder> -P check_exports_self_test.cmake
# The library defines exports_self_test_kept and exports_self_test_extra.
if(NOT NM OR NOT LIBRARY OR NOT SCRIPT OR NOT WORK)
    message(FATAL_ERROR "check_exports_self_test.cmake: give -DNM, -DLIBRARY, -DSCRIPT and -DWORK")
endif()

function(check_exports names expected_status expected_text)
    file(WRITE "${WORK}/exports_self_test.txt" "${names}")
    execute_process(COMMAND ${CMAKE_COMMAND} -DNM=${NM} "-DCHECKS=${LIBRARY}|${WORK}/exports_self_test.txt" -P ${SCRIPT}
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(expected_status STREQUAL "0")
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "a file that lists both names was refused (exit ${status}):\n${out}${err}")
        endif()
    else()
        if(status EQUAL 0)
            message(FATAL_ERROR "a wrong export file (${expected_text}) was accepted:\n${out}")
        endif()
        if(NOT "${out}${err}" MATCHES "${expected_text}")
            message(FATAL_ERROR "the refusal does not say '${expected_text}':\n${out}${err}")
        endif()
    endif()
endfunction()

check_exports("exports_self_test_kept\nexports_self_test_extra\n" 0 "")
check_exports("exports_self_test_kept\n" 1 "does not list")
check_exports("exports_self_test_kept\nexports_self_test_extra\nexports_self_test_missing\n" 1 "does not export")
message(STATUS "check_exports.cmake accepts the right export file and refuses a missing and an extra name")
