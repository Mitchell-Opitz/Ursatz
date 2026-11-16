cmake_minimum_required(VERSION 3.20)
project(ursatz-gui C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# The repo root adds cJSON, the library, the Analyzer, the Composer and civetweb first; the GUI links their targets.
# The Analyzer is the only analysis path (R-084); every Composer request calls the Composer API in process (R-219): the
# quick ones on request threads, the long runs on the job queue's workers.
# The library checkout, for tests that read its fixtures.
set(URSATZ_GUI_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/library)

add_subdirectory(backend)

if(URSATZ_TESTS)
  add_subdirectory(backend/tests)
  add_subdirectory(frontend/tests)

  # Default timeouts by label: 30 s, or 1800 s for tests labeled slow.
  include(cmake/TestTimeouts.cmake)
  ursatz_gui_apply_test_timeouts(${CMAKE_CURRENT_SOURCE_DIR}/backend/tests)
  ursatz_gui_apply_test_timeouts(${CMAKE_CURRENT_SOURCE_DIR}/frontend/tests)
endif()
