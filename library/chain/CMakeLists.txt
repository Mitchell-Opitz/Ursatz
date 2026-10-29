# Analysis chain: an extension above the kernel and the frameworks that joins
# them. Frameworks never compile kernel sources; anything that needs both lives
# here (R-241). Sources are added as the chain grows.
add_library(ursatz_chain
    ursatz_chain/form_bridge.c
    ursatz_chain/subset_chord_analyzer.c
    ursatz_chain/key_collection_analyzer.c
    ursatz_chain/chain_inputs.c
    ursatz_chain/kernel_owners.c
    ursatz_chain/analysis_chain.c
)
# Public headers (R-223), staged under ursatz_chain/ in the build tree; the chain directory is private.
ursatz_public_headers(ursatz_chain
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/ursatz_chain
    PREFIX ursatz_chain
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS
        analysis_chain.h
        chain_inputs.h
        form_bridge.h
        kernel_owners.h
        key_collection_analyzer.h
        subset_chord_analyzer.h
)
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
