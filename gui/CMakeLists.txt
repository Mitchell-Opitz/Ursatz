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
# Same cross-repo pattern as ursatz-analyzer: prefer a local checkout via
# URSATZ_SOURCE_DIR when developing against an in-progress Ursatz tree,
# otherwise fetch a pinned tag. Guard against CMAKE_SOURCE_DIR vs
# PROJECT_SOURCE_DIR diverging when this project is consumed as a
# subdirectory of something else rather than built standalone.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(URSATZ_GUI_STANDALONE ON)
else()
  set(URSATZ_GUI_STANDALONE OFF)
endif()

set(URSATZ_SOURCE_DIR "" CACHE PATH "Path to a local Ursatz checkout (overrides fetching)")
set(URSATZ_GIT_REPOSITORY "https://github.com/mitchell-opitz/ursatz.git" CACHE STRING "Ursatz git repository")
set(URSATZ_GIT_TAG "main" CACHE STRING "Ursatz git tag/branch to fetch")

if(URSATZ_SOURCE_DIR)
  add_subdirectory(${URSATZ_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_ursatz)
else()
  FetchContent_Declare(
    ursatz
    GIT_REPOSITORY ${URSATZ_GIT_REPOSITORY}
    GIT_TAG ${URSATZ_GIT_TAG}
  )
  FetchContent_MakeAvailable(ursatz)
endif()

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
