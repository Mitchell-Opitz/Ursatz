add_library(standard_texture
    framework.c
    rule_pair.c
    texture_rules.c
    texture_classification_rules.c
    mixed_texture_rules.c
)
target_include_directories(standard_texture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_texture PUBLIC ursatz)

ursatz_add_tests()
