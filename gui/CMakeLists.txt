cmake_minimum_required(VERSION 3.20)
project(ursatz-gui C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)

# --- Library, Analyzer and Composer ------------------------------------------
#
# The repo root adds cJSON, the library, the Analyzer and the Composer first and the GUI reuses those targets; on its
# own the GUI adds its siblings without their tests or command. cJSON comes first so the Analyzer reuses it, the
# library before the Analyzer and the Analyzer before the Composer for the same reason. The Analyzer is the only
# analysis path (R-084); every Composer request calls the Composer API in process (R-219): the quick ones on request
# threads, the long runs on the job queue's workers.
set(URSATZ_BUILD_TESTS OFF)
set(URSATZ_ANALYZER_BUILD_TESTS OFF)
set(URSATZ_COMPOSER_BUILD_TESTS OFF)
set(URSATZ_COMPOSER_BUILD_CLI OFF)
if(NOT TARGET cjson)
  add_subdirectory(external/cjson)
endif()
if(NOT TARGET ursatz)
  add_subdirectory(${PROJECT_SOURCE_DIR}/../library ${PROJECT_BINARY_DIR}/library)
endif()
if(NOT TARGET ursatz_analyzer)
  add_subdirectory(${PROJECT_SOURCE_DIR}/../analyzer ${PROJECT_BINARY_DIR}/analyzer)
endif()
if(NOT TARGET ursatz_composer)
  add_subdirectory(${PROJECT_SOURCE_DIR}/../composer ${PROJECT_BINARY_DIR}/composer)
endif()
# The library checkout, for tests that read its fixtures.
set(URSATZ_GUI_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/../library)

# --- civetweb (HTTP server library) ---------------------------------------
set(CIVETWEB_ENABLE_CXX OFF CACHE BOOL "" FORCE)
set(CIVETWEB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CIVETWEB_ENABLE_SSL OFF CACHE BOOL "" FORCE)
# civetweb defaults this ON for Debug builds, which instruments it with
# ASan/UBSan without linking the sanitizer runtime into consumers of the
# library, breaking the final link. We don't want civetweb built with
# sanitizers baked in as a fixed default, so turn it off explicitly.
set(CIVETWEB_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
# civetweb v1.16 declares cmake_minimum_required below 3.5, which CMake 4
# rejects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# civetweb builds its request threads with a 100 KB stack by default. The Composer API runs on those threads (R-219)
# and some of its entries need more (a random motive draws into several motives on the stack), so a request crashed the
# server. 32 MB is address space only; the pages are used when a call needs them.
set(CIVETWEB_THREAD_STACK_SIZE 33554432 CACHE STRING "" FORCE)

FetchContent_Declare(
  civetweb
  GIT_REPOSITORY https://github.com/civetweb/civetweb.git
  GIT_TAG v1.16
)
FetchContent_MakeAvailable(civetweb)

add_subdirectory(backend)

enable_testing()
add_subdirectory(backend/tests)

add_subdirectory(frontend/tests)

# Default timeouts by label: 30 s, or 1800 s for tests labeled slow.
include(cmake/TestTimeouts.cmake)
ursatz_gui_apply_test_timeouts(${CMAKE_CURRENT_SOURCE_DIR}/backend/tests)
ursatz_gui_apply_test_timeouts(${CMAKE_CURRENT_SOURCE_DIR}/frontend/tests)
