add_library(standard_form
    analysis/analysis_input.c
    analysis/analysis_claim.c
    analysis/analysis_phrase.c
    analysis/analysis_phrase_grid.c
    analysis/analysis_repeats.c
    analysis/analysis_similarity.c
    analysis/analysis_ideas.c
    analysis/analysis_theme.c
    analysis/analysis_expansion.c
    analysis/analysis_motive_shape.c
    analysis/analysis_motive_match.c
    analysis/analysis_motive.c
)
# Public headers (R-223), staged under standard-form/ in the build tree; the source directories are private.
ursatz_public_headers(standard_form
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-form
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/analysis_expansion.h
        analysis/analysis_ideas.h
        analysis/analysis_input.h
        analysis/analysis_motive.h
        analysis/analysis_motive_match.h
        analysis/analysis_motive_shape.h
        analysis/analysis_phrase.h
        analysis/analysis_phrase_grid.h
        analysis/analysis_repeats.h
        analysis/analysis_similarity.h
        analysis/analysis_theme.h
)
target_link_libraries(standard_form PUBLIC ursatz standard_harmony)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(standard_form PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
