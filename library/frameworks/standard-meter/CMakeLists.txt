add_library(standard_meter
    framework.c
    rule_pair.c
    grouping_rules.c
    time_signature_rules.c
    measure_rules.c
    anacrusis_rules.c
    counting_rules.c
    beaming_rules.c
)
target_include_directories(standard_meter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_meter PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
