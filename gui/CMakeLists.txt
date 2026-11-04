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
# The GUI vendors cJSON. It comes first so the Analyzer reuses this target
# instead of adding its own.
add_subdirectory(external/cjson)
add_subdirectory(external/ursatz)

# --- Ursatz Analyzer dependency ------------------------------------------
#
# The Analyzer is the only analysis path (R-084). It skips its own
# add_subdirectory(external/ursatz) because the ursatz target already
# exists here.
set(URSATZ_ANALYZER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/ursatz-analyzer)

# --- Ursatz Composer (linked) ----------------------------------------------
#
# The quick requests call the Composer API in process (R-219). The Composer
# reuses the cJSON, Ursatz and Analyzer targets above; the command is not
# built here (the long runs still use the one built below until B7).
set(URSATZ_COMPOSER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(URSATZ_COMPOSER_BUILD_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(external/ursatz-composer)

# --- Ursatz Composer (built on its own) ------------------------------------
#
# The long runs (theme, piece, variations, variants) still run the Composer as
# a command until B7. It brings its own Ursatz and Analyzer, so it is built as
# an external project in its own build tree: its targets never meet the ones
# above. composer_service.c starts the binary named here, or the one in
# URSATZ_GUI_COMPOSER at run time.
#
# BUILD_ALWAYS ON runs the Composer's own (incremental) build on every GUI
# build. Without it the sub-build ran once, so a moved submodule left an old
# Composer binary behind and motifs saved without analysis (R-182).
include(ExternalProject)
set(URSATZ_GUI_COMPOSER_BINARY_DIR ${CMAKE_BINARY_DIR}/composer)
set(URSATZ_GUI_COMPOSER_BINARY ${URSATZ_GUI_COMPOSER_BINARY_DIR}/cli/ursatz-composer)
ExternalProject_Add(ursatz_composer_cli
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/ursatz-composer
  BINARY_DIR ${URSATZ_GUI_COMPOSER_BINARY_DIR}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug -DURSATZ_COMPOSER_BUILD_TESTS=OFF
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target ursatz-composer --parallel
  INSTALL_COMMAND ""
  BUILD_ALWAYS ON
  BUILD_BYPRODUCTS ${URSATZ_GUI_COMPOSER_BINARY}
)
add_compile_definitions(URSATZ_GUI_COMPOSER_BIN="${URSATZ_GUI_COMPOSER_BINARY}")

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
