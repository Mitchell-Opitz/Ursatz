add_library(standard_markings
    framework.c
    dynamics_rules.c
    articulation_rules.c
    tempo_rules.c
    structural_rules.c
    repeat_navigation.c
)
# Public headers (R-223), staged under standard-markings/ in the build tree; the source directories are private.
ursatz_public_headers(standard_markings
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-markings
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS
        framework.h
        dynamics_rules.h
        tempo_rules.h
)
target_link_libraries(standard_markings PUBLIC ursatz)

ursatz_add_tests()
