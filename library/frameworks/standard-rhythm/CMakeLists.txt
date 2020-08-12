add_library(standard_rhythm
    framework.c
    rule_pair.c
    name_rules.c
    hierarchy_rules.c
    notehead_rules.c
    rest_rules.c
    dot_rules.c
    tie_rules.c
)
target_include_directories(standard_rhythm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_rhythm PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
