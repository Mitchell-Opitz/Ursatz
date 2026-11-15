cmake_minimum_required(VERSION 3.20)
project(UrsatzComposer VERSION 0.4.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(URSATZ_COMPOSER_TESTS_DEFAULT ON)
else()
    set(URSATZ_COMPOSER_TESTS_DEFAULT OFF)
endif()
option(URSATZ_COMPOSER_BUILD_TESTS "Build and register composer tests" ${URSATZ_COMPOSER_TESTS_DEFAULT})
option(URSATZ_COMPOSER_BUILD_CLI "Build the ursatz-composer command" ${URSATZ_COMPOSER_TESTS_DEFAULT})

if(URSATZ_COMPOSER_BUILD_TESTS)
    enable_testing()
endif()

# The repo root adds cJSON, the library and the Analyzer first and the Composer reuses those targets; on its own the
# Composer adds its siblings without their tests, cJSON first so the Analyzer reuses it instead of adding its own, and
# the library before the Analyzer for the same reason.
set(URSATZ_BUILD_TESTS OFF)
set(URSATZ_ANALYZER_BUILD_TESTS OFF)
if(NOT TARGET cjson)
    add_subdirectory(external/cjson)
endif()
if(NOT TARGET ursatz)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../library ${PROJECT_BINARY_DIR}/library)
endif()
if(NOT TARGET ursatz_analyzer)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../analyzer ${PROJECT_BINARY_DIR}/analyzer)
endif()
# Sibling headers are not ours to warn about.
foreach(submodule_target ursatz ursatz_analyzer common_practice_minimal standard_meter standard_tonality standard_markings standard_harmony standard_counterpoint standard_form standard_texture standard_voicing galant_schemas scale_harmonization cjson)
    get_target_property(submodule_includes ${submodule_target} INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(${submodule_target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${submodule_includes}")
endforeach()
add_subdirectory(src)
if(URSATZ_COMPOSER_BUILD_CLI)
    add_subdirectory(cli)
endif()
if(URSATZ_COMPOSER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
