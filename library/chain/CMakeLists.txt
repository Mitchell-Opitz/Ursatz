# Analysis chain: an extension above the kernel and the frameworks that joins
# them. Frameworks never compile kernel sources; anything that needs both lives
# here (R-241). Sources are added as the chain grows.
add_library(ursatz_chain
    ursatz_chain/form_bridge.c
    ursatz_chain/subset_chord_analyzer.c
    ursatz_chain/key_collection_analyzer.c
    ursatz_chain/chain_inputs.c
    ursatz_chain/analysis_chain.c
)
target_include_directories(ursatz_chain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz_chain PUBLIC
    ursatz
    common_practice_minimal
    standard_tonality
    standard_harmony
    standard_form
    standard_meter
    standard_texture
    standard_counterpoint
    galant_schemas
    scale_harmonization
)

ursatz_add_tests()
