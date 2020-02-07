add_library(common_practice_minimal
    framework.c
    chord_vocabulary.c
    chord_vocabulary_seventh.c
    key_vocabulary.c
    triad_identification.c
    seventh_identification.c
    key_identification.c
    ${PROJECT_SOURCE_DIR}/src/analysis/chord_identification.c
    ${PROJECT_SOURCE_DIR}/src/analysis/key_estimation.c
)
target_include_directories(common_practice_minimal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(common_practice_minimal PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()