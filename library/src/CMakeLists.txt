add_library(ursatz
    ursatz.c
    identity/entity_id.c
    identity/version.c
    identity/version_model.c
    identity/schema_version.c
    core/entity.c
    core/value.c
    core/occurrence.c
)
target_include_directories(ursatz PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)