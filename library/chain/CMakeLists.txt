# Analysis chain: an extension above the model, theory and analysis groups that joins them
# (R-241). Sources are added as the chain grows.
add_library(ursatz_chain
    analysis_chain.c
    chain_inputs.c
    form_bridge.c
    kernel_owners.c
)
# Public headers (R-223), staged under chain/ in the build tree; the chain directory is private.
ursatz_public_headers(ursatz_chain
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        chain/analysis_chain.h
        chain/chain_inputs.h
)
target_link_libraries(ursatz_chain PUBLIC ursatz_analysis)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
