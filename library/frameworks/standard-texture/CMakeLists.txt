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
target_include_directories(standard_texture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_texture PUBLIC ursatz)

ursatz_add_tests()
