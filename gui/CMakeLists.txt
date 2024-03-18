cmake_minimum_required(VERSION 3.16)
project(ursatz-gui C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)

# --- Ursatz library dependency -------------------------------------------
#
# Same cross-repo pattern as ursatz-analyzer: Ursatz is vendored as a git
# submodule under external/ursatz and pulled in directly with
# add_subdirectory. Ursatz's own CMakeLists.txt guards its test suite with
# `if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)` so it doesn't
# try to build its tests when consumed this way.
add_subdirectory(external/ursatz)

# --- civetweb (HTTP server library) ---------------------------------------
set(CIVETWEB_ENABLE_CXX OFF CACHE BOOL "" FORCE)
set(CIVETWEB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CIVETWEB_ENABLE_SSL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  civetweb
  GIT_REPOSITORY https://github.com/civetweb/civetweb.git
  GIT_TAG v1.16
)
FetchContent_MakeAvailable(civetweb)

add_subdirectory(backend)

enable_testing()
add_subdirectory(backend/tests)
