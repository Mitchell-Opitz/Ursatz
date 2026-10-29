add_library(standard_voicing
    framework.c
    voicing_pitch.c
    voicing_rules.c
    stem_rules.c
    construction_rules.c
    range_rules.c
    spacing_rules.c
    crossing_rules.c
    doubling_rules.c
)
# Public headers (R-223), staged under standard-voicing/ in the build tree; the source directories are private.
ursatz_public_headers(standard_voicing
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-voicing
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS
        range_rules.h
        spacing_rules.h
        stem_rules.h
        voicing_chord.h
        voicing_pitch.h
        voicing_rules.h
)
target_link_libraries(standard_voicing PUBLIC ursatz)

ursatz_add_tests()
