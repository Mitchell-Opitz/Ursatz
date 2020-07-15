add_library(standard_markings
    framework.c
    rule_pair.c
    dynamics_rules.c
    articulation_rules.c
    tempo_rules.c
    structural_rules.c
    repeat_navigation.c
)
target_include_directories(standard_markings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_markings PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
