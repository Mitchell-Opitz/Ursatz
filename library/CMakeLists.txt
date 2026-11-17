# The library is built from the repository root (the root sets the C standard, the build type and URSATZ_TESTS).
project(Ursatz C)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UrsatzPublicHeaders.cmake)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(model)
add_subdirectory(theory)
add_subdirectory(analysis)
add_subdirectory(src)
add_subdirectory(chain)

ursatz_write_public_manifest()
if(URSATZ_TESTS)
    ursatz_add_public_header_tests()
endif()
