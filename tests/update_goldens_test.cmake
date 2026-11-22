# Checks that cmake/UpdateGoldens.cmake passes when the second ctest run passes and fails when it fails, with `true` and `false` standing
# in for ctest (a script that always exits 0 would update goldens that do not hold).
#   cmake -DSCRIPT=<cmake/UpdateGoldens.cmake> [-DWORK=<folder>] -P update_goldens_test.cmake
if(NOT SCRIPT)
    message(FATAL_ERROR "update_goldens_test.cmake: give -DSCRIPT")
endif()
find_program(TRUE_PROGRAM true REQUIRED)
find_program(FALSE_PROGRAM false REQUIRED)
if(NOT WORK)
    set(WORK "${CMAKE_CURRENT_LIST_DIR}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -DCTEST=${TRUE_PROGRAM} -DBUILD_DIR=${WORK} -P ${SCRIPT}
                RESULT_VARIABLE passing OUTPUT_VARIABLE passing_out ERROR_VARIABLE passing_err)
if(NOT passing EQUAL 0)
    message(FATAL_ERROR "UpdateGoldens.cmake failed when the goldens pass (exit ${passing}):\n${passing_out}${passing_err}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -DCTEST=${FALSE_PROGRAM} -DBUILD_DIR=${WORK} -P ${SCRIPT}
                RESULT_VARIABLE failing OUTPUT_VARIABLE failing_out ERROR_VARIABLE failing_err)
if(failing EQUAL 0)
    message(FATAL_ERROR "UpdateGoldens.cmake passed when the goldens do not pass:\n${failing_out}${failing_err}")
endif()
message(STATUS "UpdateGoldens.cmake passes on a passing run and fails on a failing one")
