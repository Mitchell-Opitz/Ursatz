add_library(standard_rhythm
    framework.c
    name_rules.c
    hierarchy_rules.c
    notehead_rules.c
    rest_rules.c
    dot_rules.c
    tie_rules.c
    tuplet_rules.c
)
# No app, chain or framework source includes a header of standard-rhythm yet, so nothing is public (R-223).
ursatz_public_headers(standard_rhythm
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-rhythm
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(standard_rhythm PUBLIC ursatz)

ursatz_add_tests()
