cmake_minimum_required(VERSION 3.20)
project(ursatz-gui C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The repo root adds cJSON, the library, the Analyzer, the Composer and civetweb first; the GUI links their targets.
# The Analyzer is the only analysis path (R-084); every Composer request calls the Composer API in process (R-219): the
# quick ones on request threads, the long runs on the job queue's workers.
# The library checkout, for tests that read its fixtures.
set(URSATZ_GUI_LIBRARY_DIR ${CMAKE_SOURCE_DIR})

add_subdirectory(backend)

if(URSATZ_TESTS)
  add_subdirectory(backend/tests)
  add_subdirectory(frontend/tests)
endif()
