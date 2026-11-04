# Builds the consumer targets of tests/consumer. Needs -DBUILD_DIR. The plain consumer and the chain consumer must build;
# the two leak consumers must fail with a missing header, not for another reason.
function(build_target target out_result out_log)
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR} --target ${target}
                    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE log)
    set(${out_result} ${result} PARENT_SCOPE)
    set(${out_log} "${log}" PARENT_SCOPE)
endfunction()

foreach(target analyzer_consumer_plain analyzer_consumer_chain)
    build_target(${target} result log)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${target} must build:\n${log}")
    endif()
endforeach()

foreach(target analyzer_consumer_leak_framework analyzer_consumer_leak_chain)
    build_target(${target} result log)
    if(result EQUAL 0)
        message(FATAL_ERROR "${target} built: a target linking only ursatz_analyzer can see a chain or framework header")
    endif()
    if(NOT log MATCHES "No such file or directory")
        message(FATAL_ERROR "${target} failed for another reason:\n${log}")
    endif()
endforeach()
message(STATUS "consumer isolation holds")
