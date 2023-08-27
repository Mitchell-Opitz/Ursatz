add_library(common_practice_minimal
    framework.c
    chord_vocabulary.c
    chord_vocabulary_seventh.c
    key_vocabulary.c
    triad_identification.c
    seventh_identification.c
    key_identification.c
    weighted_identification.c
    non_chord_tones.c
    weighted_chord_identification.c
    outer_scale_degrees.c
    key_regions.c
    ${PROJECT_SOURCE_DIR}/src/analysis/chord_identification.c
    ${PROJECT_SOURCE_DIR}/src/analysis/key_estimation.c
)
target_include_directories(common_practice_minimal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(common_practice_minimal PUBLIC ursatz)

ursatz_add_tests()