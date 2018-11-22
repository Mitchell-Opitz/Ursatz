add_library(ursatz
    ursatz.c
)

target_include_directories(ursatz PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)