add_library(galant_schemas
    analysis/galant_analysis_input.c
    analysis/galant_analysis_claim.c
    analysis/galant_analysis_match.c
)
# Public headers (R-223), staged under galant-schemas/ in the build tree; the source directories are private.
ursatz_public_headers(galant_schemas
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX galant-schemas
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/galant_analysis_input.h
        analysis/galant_analysis_match.h
)
target_link_libraries(galant_schemas PUBLIC standard_counterpoint ursatz)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(galant_schemas PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
