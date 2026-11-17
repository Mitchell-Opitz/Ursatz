add_library(standard_meter
    analysis/meter_analysis_position.c
    analysis/meter_analysis_input.c
    analysis/meter_analysis_claim.c
    analysis/meter_analysis_syncopation.c
    analysis/meter_analysis_hemiola.c
    analysis/meter_analysis_hypermeter.c
    analysis/meter_analysis_density.c
    analysis/meter_analysis_check.c
    analysis/meter_analysis_tuplet.c
)
# Public headers (R-223), staged under standard-meter/ in the build tree; the source directories are private.
ursatz_public_headers(standard_meter
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-meter
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/meter_analysis_check.h
        analysis/meter_analysis_density.h
        analysis/meter_analysis_hemiola.h
        analysis/meter_analysis_hypermeter.h
        analysis/meter_analysis_input.h
        analysis/meter_analysis_syncopation.h
        analysis/meter_analysis_position.h
        analysis/meter_analysis_tuplet.h
)
target_link_libraries(standard_meter PUBLIC ursatz)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(standard_meter PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
