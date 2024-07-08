cmake_minimum_required(VERSION 3.20)
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
set(URSATZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/ursatz)

# --- Ursatz Analyzer dependency ------------------------------------------
#
# The Analyzer is the only analysis path (R-084). It skips its own
# add_subdirectory(external/ursatz) because the ursatz target already
# exists here.
set(URSATZ_ANALYZER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/ursatz-analyzer)

# --- civetweb (HTTP server library) ---------------------------------------
set(CIVETWEB_ENABLE_CXX OFF CACHE BOOL "" FORCE)
set(CIVETWEB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CIVETWEB_ENABLE_SSL OFF CACHE BOOL "" FORCE)
# civetweb defaults this ON for Debug builds, which instruments it with
# ASan/UBSan without linking the sanitizer runtime into consumers of the
# library, breaking the final link. We don't want civetweb built with
# sanitizers baked in as a fixed default, so turn it off explicitly.
set(CIVETWEB_ENABLE_ASAN OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  civetweb
  GIT_REPOSITORY https://github.com/civetweb/civetweb.git
  GIT_TAG v1.16
)
FetchContent_MakeAvailable(civetweb)

# The Analyzer defines its version privately; read it from its project()
# line so meta.json can record it.
file(STRINGS external/ursatz-analyzer/CMakeLists.txt URSATZ_GUI_ANALYZER_PROJECT_LINE
     REGEX "^project\\(UrsatzAnalyzer VERSION [0-9.]+")
if(URSATZ_GUI_ANALYZER_PROJECT_LINE MATCHES "VERSION ([0-9.]+)")
  add_compile_definitions(URSATZ_ANALYZER_VERSION="${CMAKE_MATCH_1}")
endif()

add_subdirectory(backend)

enable_testing()
add_subdirectory(backend/tests)

add_subdirectory(frontend/tests)
