add_library(ursatz
    ursatz.c
    identity/entity_id.c
    identity/version.c
)
target_include_directories(ursatz PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)