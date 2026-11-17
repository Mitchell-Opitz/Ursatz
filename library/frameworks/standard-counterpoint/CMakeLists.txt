add_library(standard_counterpoint
    analysis/analysis_grid.c
    analysis/analysis_claim.c
    analysis/analysis_motion.c
    analysis/analysis_perfects.c
    analysis/analysis_melody_emit.c
    analysis/analysis_melody.c
    analysis/analysis_melody_climax.c
    analysis/analysis_melody_leaps.c
    analysis/analysis_melody_nct.c
)
# Public headers (R-223), staged under standard-counterpoint/ in the build tree; the source directories are private.
ursatz_public_headers(standard_counterpoint
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-counterpoint
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/analysis_grid.h
        analysis/analysis_melody.h
        analysis/analysis_melody_climax.h
        analysis/analysis_melody_leaps.h
        analysis/analysis_melody_nct.h
        analysis/analysis_motion.h
        analysis/analysis_perfects.h
)
target_link_libraries(standard_counterpoint PUBLIC ursatz)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(standard_counterpoint PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
