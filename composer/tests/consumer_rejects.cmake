# Builds each reject_* target of the consumer project and requires the compile to fail because the header is not found.
# Arguments: -DBUILD_DIR=<the consumer build directory> -DTARGETS=<target;target;...>
foreach(target ${TARGETS})
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR} --target ${target}
                    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(result EQUAL 0)
        message(FATAL_ERROR "${target} compiled: a consumer sees a header it must not")
    endif()
    if(NOT "${out}${err}" MATCHES "fatal error: [^\n]*No such file or directory")
        message(FATAL_ERROR "${target} failed for another reason:\n${out}\n${err}")
    endif()
    message(STATUS "${target} refused, as required")
endforeach()
