add_library(standard_tonality
    framework.c
    tonal_pitch.c
    tonal_key.c
    scale_pattern.c
    major_scale_rules.c
    minor_scale_rules.c
    mode_rules.c
    pentatonic_rules.c
    chromatic_scale_rules.c
    interval_size_rules.c
    interval_quality_rules.c
    interval_inversion_rules.c
    interval_consonance_rules.c
    interval_white_key_rules.c
    interval_enharmonic_rules.c
    interval_spelling_rules.c
    triad_rules.c
    triad_spelling_rules.c
    triad_identification_rules.c
    chord_symbol_rules.c
    triad_in_key_rules.c
    modal_solfege_rules.c
    solfege_rules.c
    scale_degree_rules.c
    minor_scale_degree_rules.c
    key_relationship_rules.c
    minor_key_rules.c
    key_signature_rules.c
    circle_of_fifths_rules.c
    seventh_rules.c
    seventh_spelling_rules.c
    seventh_identification_rules.c
    seventh_in_key_rules.c
    roman_numeral_rules.c
    roman_numeral_grammar.c
    roman_numeral_analysis_rules.c
    analysis/chord_input.c
    analysis/roman_numeral_adapter.c
    analysis/applied_chords.c
)
# Public headers (R-223), staged under standard-tonality/ in the build tree; the source directories are private.
ursatz_public_headers(standard_tonality
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-tonality
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/applied_chords.h
        analysis/chord_input.h
        analysis/roman_numeral_adapter.h
        chord_symbol_rules.h
        interval_quality_rules.h
        minor_scale_rules.h
        roman_numeral_analysis_rules.h
        roman_numeral_grammar.h
        roman_numeral_rules.h
        seventh_rules.h
        tonal_key.h
        triad_rules.h
)
target_link_libraries(standard_tonality PUBLIC ursatz)

ursatz_add_tests()
