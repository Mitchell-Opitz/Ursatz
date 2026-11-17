add_library(common_practice_minimal
    key_identification.c
    weighted_identification.c
    non_chord_tones.c
    weighted_chord_identification.c
    outer_scale_degrees.c
    key_regions.c
    harmonic_spans.c
)
# Public headers (R-223), staged under common-practice-minimal/ in the build tree; the source directories are private.
ursatz_public_headers(common_practice_minimal
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX common-practice-minimal
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS
        harmonic_spans.h
        key_identification.h
        key_regions.h
        non_chord_tones.h
        outer_scale_degrees.h
        weighted_chord_identification.h
        weighted_identification.h
)
target_link_libraries(common_practice_minimal PUBLIC standard_tonality ursatz m)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(common_practice_minimal PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()