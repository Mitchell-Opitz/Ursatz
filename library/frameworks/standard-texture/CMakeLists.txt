add_library(standard_texture
    framework.c
    texture_rules.c
    texture_classification_rules.c
    mixed_texture_rules.c
    analysis/texture_analysis_time.c
    analysis/texture_analysis_input.c
    analysis/texture_analysis_features.c
    analysis/texture_analysis_claim.c
    analysis/texture_analysis_classify.c
)
# Public headers (R-223), staged under standard-texture/ in the build tree; the source directories are private.
ursatz_public_headers(standard_texture
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-texture
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/texture_analysis_classify.h
        analysis/texture_analysis_features.h
        analysis/texture_analysis_input.h
        analysis/texture_analysis_time.h
        framework.h
        texture_classification_rules.h
        texture_rules.h
)
target_link_libraries(standard_texture PUBLIC ursatz)

ursatz_add_tests()
