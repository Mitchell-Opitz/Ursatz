add_library(scale_harmonization
    analysis/scale_analysis_claim.c
    analysis/scale_analysis_detect.c
)
# Public headers (R-223), staged under scale-harmonization/ in the build tree; the source directories are private.
ursatz_public_headers(scale_harmonization
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX scale-harmonization
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/scale_analysis_detect.h
)
target_link_libraries(scale_harmonization PUBLIC galant_schemas standard_counterpoint ursatz)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(scale_harmonization PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
