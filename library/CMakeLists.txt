cmake_minimum_required(VERSION 3.20)
project(Ursatz C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(URSATZ_TESTS_DEFAULT ON)
else()
    set(URSATZ_TESTS_DEFAULT OFF)
endif()
option(URSATZ_BUILD_TESTS "Build and register kernel and framework tests" ${URSATZ_TESTS_DEFAULT})

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UrsatzTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UrsatzPublicHeaders.cmake)

if(URSATZ_BUILD_TESTS)
    enable_testing()
endif()
ursatz_add_tests()

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(frameworks)
add_subdirectory(chain)
