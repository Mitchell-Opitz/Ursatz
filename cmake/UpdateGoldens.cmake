# Rewrites every golden file: the one place that says how (the update-goldens target of the root CMakeLists.txt runs it).
#
#     cmake -DCTEST=<ctest> -DBUILD_DIR=<build directory> -P cmake/UpdateGoldens.cmake
#
# 1. Runs the tests labelled golden with URSATZ_UPDATE_GOLDENS=1 in the environment. Each of them writes its goldens and
#    then fails on purpose (tests/support/ursatz_test.h), so a run with the switch can never pass in CI; the exit code
#    of this run is ignored.
# 2. Runs the tests labelled golden again without the switch, against the files just written, and fails when that run
#    fails (an unstable golden, or a test that broke for another reason).
#
# The tests are not built here: build the tree first (cmake --build <build directory>), or the tests that run are the
# ones built last. The files change in the work tree; review them with git diff and commit them in their own commit.

foreach(variable CTEST BUILD_DIR)
    if(NOT ${variable})
        message(FATAL_ERROR "UpdateGoldens.cmake: pass -D${variable}=<value>")
    endif()
endforeach()

cmake_host_system_information(RESULT jobs QUERY NUMBER_OF_LOGICAL_CORES)

message(STATUS "Updating the goldens: every test of this run fails on purpose (URSATZ_UPDATE_GOLDENS=1)")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env URSATZ_UPDATE_GOLDENS=1 ${CTEST} --test-dir ${BUILD_DIR} -L golden -j${jobs}
    RESULT_VARIABLE update_status)

message(STATUS "Checking the updated goldens")
execute_process(
    COMMAND ${CTEST} --test-dir ${BUILD_DIR} -L golden --output-on-failure
    RESULT_VARIABLE check_status)
if(NOT check_status EQUAL 0)
    message(FATAL_ERROR "The goldens do not pass after the update (ctest -L golden exit ${check_status}); see above")
endif()
message(STATUS "Goldens updated; review them with git diff")
