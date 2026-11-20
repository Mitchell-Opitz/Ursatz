cmake_minimum_required(VERSION 3.28)
project(Ursatz LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Debug unless asked otherwise: Release compiles asserts out and the tests rely on them.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(URSATZ_GUI "Build the GUI and its HTTP server" ON)
option(URSATZ_TESTS "Build and register the tests of every part" ON)

enable_testing()
include(cmake/UrsatzTools.cmake)
include(cmake/UrsatzTest.cmake)
include(cmake/UrsatzExports.cmake)

# Dependencies first; each later part reuses the targets the earlier ones define.
add_subdirectory(third_party)
# common/ stages its headers with the library's ursatz_public_headers helper, so the helper is defined first.
include(library/cmake/UrsatzPublicHeaders.cmake)
add_subdirectory(common)
add_subdirectory(library)
add_subdirectory(analyzer)
add_subdirectory(composer)
add_subdirectory(workshop)
add_subdirectory(cli)
if(URSATZ_GUI)
    add_subdirectory(gui)
endif()
if(URSATZ_TESTS)
    add_subdirectory(tests)
    # cmake --build build --target update-goldens: rewrites every golden (cmake/UpdateGoldens.cmake).
    add_custom_target(update-goldens
        COMMAND ${CMAKE_COMMAND} -DCTEST=${CMAKE_CTEST_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_SOURCE_DIR}/cmake/UpdateGoldens.cmake
        USES_TERMINAL)
endif()
