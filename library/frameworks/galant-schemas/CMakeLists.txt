add_library(galant_schemas
    galant_framework.c
    galant_rule_pair.c
    galant_rule_table.c
    galant_degree.c
    galant_exemplar.c
    galant_prototypes.c
)
target_include_directories(galant_schemas PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(galant_schemas PUBLIC standard_counterpoint ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
