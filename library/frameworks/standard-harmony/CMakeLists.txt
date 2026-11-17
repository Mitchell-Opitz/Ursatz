add_library(standard_harmony
    analysis/analysis_input.c
    analysis/analysis_claim.c
    analysis/analysis_function.c
    analysis/analysis_phrase.c
    analysis/analysis_six_four.c
    analysis/analysis_prolongation.c
    analysis/analysis_resolution.c
    analysis/analysis_runs.c
    analysis/analysis_cadence.c
    analysis/analysis_cadence_readings.c
    analysis/analysis_cadence_claims.c
)
# Public headers (R-223), staged under standard-harmony/ in the build tree; the source directories are private.
ursatz_public_headers(standard_harmony
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-harmony
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/analysis_cadence.h
        analysis/analysis_cadence_claims.h
        analysis/analysis_function.h
        analysis/analysis_input.h
        analysis/analysis_phrase.h
        analysis/analysis_prolongation.h
        analysis/analysis_resolution.h
        analysis/analysis_six_four.h
        analysis/analysis_voices.h
)
target_link_libraries(standard_harmony PUBLIC ursatz)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(standard_harmony PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
