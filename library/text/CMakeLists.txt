# The text group (R-662, spec/target-architecture.md 7.2): Ursatz Text, the compact lossless text form of the library's objects.
# Sources include headers by their path from library/ (text/<file>.h). A header is staged when a file outside the group includes it
# (the include closure, R-578): none is yet, the workshop stages them (P65 B3).
add_library(ursatz_text
    text.c
    text_layers.c
    text_tree.c
)
ursatz_public_headers(ursatz_text
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
)
target_link_libraries(ursatz_text PUBLIC ursatz_plan ursatz_reading ursatz_realization ursatz_theory cjson ursatz_common)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
