add_library(common_practice_minimal
    framework.c
    chord_vocabulary.c
    chord_vocabulary_seventh.c
    key_vocabulary.c
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
        chord_vocabulary.h
        chord_vocabulary_seventh.h
        framework.h
        harmonic_spans.h
        key_identification.h
        key_regions.h
        key_vocabulary.h
        non_chord_tones.h
        outer_scale_degrees.h
        weighted_chord_identification.h
        weighted_identification.h
)
target_link_libraries(common_practice_minimal PUBLIC standard_tonality ursatz m)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()