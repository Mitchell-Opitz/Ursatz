add_library(standard_notation
    framework.c
    staff_clef_rules.c
    ledger_line_rules.c
    stem_rules.c
    beam_stem_rules.c
    flag_rules.c
    second_rules.c
    octave_equivalence_rules.c
    clef_letter_rules.c
    semitone.c
    step_rules.c
    accidental_rules.c
    accidental_placement_rules.c
    enharmonic_rules.c
    key_signature_rules.c
)
# No app, chain or framework source includes a header of standard-notation yet, so nothing is public (R-223).
ursatz_public_headers(standard_notation
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-notation
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(standard_notation PUBLIC ursatz)

ursatz_add_tests()
