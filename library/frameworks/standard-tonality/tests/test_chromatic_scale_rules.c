#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chromatic_scale_rules.h"
#include "framework.h"
#include "spelling_test_util.h"

#define NOTES STANDARD_TONALITY_CHROMATIC_NOTE_COUNT

static void destroy_scale(PitchSpelling *scale[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(scale[i]);
    }
}

static void check_scale(StandardTonalityChromaticDirection direction, PitchLetter tonic_letter, int tonic_accidental,
                        const PitchLetter letters[NOTES], const int accidentals[NOTES]) {
    PitchSpelling *tonic = spell(tonic_letter, tonic_accidental, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_chromatic_scale(direction, tonic, scale));
    for (size_t i = 0; i < NOTES; i++) {
        assert(pitch_spelling_letter(scale[i]) == letters[i]);
        assert(pitch_spelling_accidental(scale[i]) == accidentals[i]);
    }
    assert(pitch_spelling_equals(scale[0], tonic));
    assert(standard_tonality_is_chromatic_scale(direction, (const PitchSpelling *const *)scale, NOTES));
    assert(standard_tonality_chromatic_spelling_conventional(direction, (const PitchSpelling *const *)scale, NOTES));
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_CHROMATIC_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_chromatic_rule((StandardTonalityChromaticRule)i);
        const Constraint *constraint = standard_tonality_chromatic_constraint((StandardTonalityChromaticRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_chromatic_rule(STANDARD_TONALITY_CHROMATIC_RULE_COUNT) == NULL);
}

/* NATURAL_HALF_STEPS: B to C and E to F. */
static void test_natural_half_steps(void) {
    assert(standard_tonality_natural_half_step_above(PITCH_LETTER_E));
    assert(standard_tonality_natural_half_step_above(PITCH_LETTER_B));
    assert(!standard_tonality_natural_half_step_above(PITCH_LETTER_C));
    assert(!standard_tonality_natural_half_step_above(PITCH_LETTER_D));
    assert(!standard_tonality_natural_half_step_above(PITCH_LETTER_F));
    assert(!standard_tonality_natural_half_step_above(PITCH_LETTER_G));
    assert(!standard_tonality_natural_half_step_above(PITCH_LETTER_A));
}

/* Example 11, C: sharps ascending. */
static void test_c_ascending(void) {
    PitchLetter letters[NOTES] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_E,
                                  PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_G, PITCH_LETTER_A,
                                  PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C};
    int accidentals[NOTES] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0};
    check_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, PITCH_LETTER_C, 0, letters, accidentals);

    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, tonic, scale));
    assert(pitch_spelling_octave(scale[12]) == 5);
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

/* Flats descending. */
static void test_c_descending(void) {
    PitchLetter letters[NOTES] = {PITCH_LETTER_C, PITCH_LETTER_B, PITCH_LETTER_B, PITCH_LETTER_A, PITCH_LETTER_A,
                                  PITCH_LETTER_G, PITCH_LETTER_G, PITCH_LETTER_F, PITCH_LETTER_E, PITCH_LETTER_E,
                                  PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_C};
    int accidentals[NOTES] = {0, 0, -1, 0, -1, 0, -1, 0, 0, -1, 0, -1, 0};
    check_scale(STANDARD_TONALITY_CHROMATIC_DESCENDING, PITCH_LETTER_C, 0, letters, accidentals);

    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_chromatic_scale(STANDARD_TONALITY_CHROMATIC_DESCENDING, tonic, scale));
    assert(pitch_spelling_octave(scale[12]) == 4); /* an octave below */
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

/* Example 11, A. */
static void test_a_ascending_and_descending(void) {
    PitchLetter up_letters[NOTES] = {PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C, PITCH_LETTER_C,
                                     PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F,
                                     PITCH_LETTER_G, PITCH_LETTER_G, PITCH_LETTER_A};
    int up_accidentals[NOTES] = {0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};
    check_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, PITCH_LETTER_A, 0, up_letters, up_accidentals);

    PitchLetter down_letters[NOTES] = {PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_G, PITCH_LETTER_G, PITCH_LETTER_F,
                                       PITCH_LETTER_E, PITCH_LETTER_E, PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_C,
                                       PITCH_LETTER_B, PITCH_LETTER_B, PITCH_LETTER_A};
    int down_accidentals[NOTES] = {0, -1, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0};
    check_scale(STANDARD_TONALITY_CHROMATIC_DESCENDING, PITCH_LETTER_A, 0, down_letters, down_accidentals);
}

/* SPELLING: a scale that begins on a flat note is written with naturals
   and sharps ascending, and flats descending. Example 11, E-flat. */
static void test_flat_tonic(void) {
    PitchLetter up_letters[NOTES] = {PITCH_LETTER_E, PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                     PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C,
                                     PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E};
    int up_accidentals[NOTES] = {-1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, -1}; /* the tonic stays E-flat */
    check_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, PITCH_LETTER_E, -1, up_letters, up_accidentals);

    PitchLetter down_letters[NOTES] = {PITCH_LETTER_E, PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_C, PITCH_LETTER_B,
                                       PITCH_LETTER_B, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_G, PITCH_LETTER_G,
                                       PITCH_LETTER_F, PITCH_LETTER_E, PITCH_LETTER_E};
    int down_accidentals[NOTES] = {-1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0, 0, -1};
    check_scale(STANDARD_TONALITY_CHROMATIC_DESCENDING, PITCH_LETTER_E, -1, down_letters, down_accidentals);
}

/* Property: from any tonic, both directions are twelve half steps whose
   last note is the tonic's pitch class an octave away. */
static void test_every_tonic(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (int direction = 0; direction < 2; direction++) {
        StandardTonalityChromaticDirection d = (StandardTonalityChromaticDirection)direction;
        for (size_t l = 0; l < 7; l++) {
            for (int accidental = -1; accidental <= 1; accidental++) {
                PitchSpelling *tonic = spell(letters[l], accidental, 4);
                PitchSpelling *scale[NOTES];
                assert(standard_tonality_build_chromatic_scale(d, tonic, scale));
                assert(standard_tonality_is_chromatic_scale(d, (const PitchSpelling *const *)scale, NOTES));
                assert(standard_tonality_chromatic_spelling_conventional(d, (const PitchSpelling *const *)scale,
                                                                         NOTES));
                assert(pitch_spelling_letter(scale[12]) == letters[l]);
                assert(pitch_spelling_accidental(scale[12]) == accidental);
                int octave_shift = pitch_spelling_octave(scale[12]) - pitch_spelling_octave(scale[0]);
                assert(octave_shift == (d == STANDARD_TONALITY_CHROMATIC_ASCENDING ? 1 : -1));
                destroy_scale(scale, NOTES);
                pitch_spelling_destroy(tonic);
            }
        }
    }
}

/* Chromatic scales are usually, not always, spelled this way: flats
   ascending is still a chromatic scale. */
static void test_other_spelling_is_still_chromatic(void) {
    PitchSpelling *scale[NOTES] = {spell(PITCH_LETTER_C, 0, 4),  spell(PITCH_LETTER_D, -1, 4), spell(PITCH_LETTER_D, 0, 4),
                                   spell(PITCH_LETTER_E, -1, 4), spell(PITCH_LETTER_E, 0, 4),  spell(PITCH_LETTER_F, 0, 4),
                                   spell(PITCH_LETTER_G, -1, 4), spell(PITCH_LETTER_G, 0, 4),  spell(PITCH_LETTER_A, -1, 4),
                                   spell(PITCH_LETTER_A, 0, 4),  spell(PITCH_LETTER_B, -1, 4), spell(PITCH_LETTER_B, 0, 4),
                                   spell(PITCH_LETTER_C, 0, 5)};
    assert(standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING,
                                                (const PitchSpelling *const *)scale, NOTES));
    assert(!standard_tonality_chromatic_spelling_conventional(STANDARD_TONALITY_CHROMATIC_ASCENDING,
                                                              (const PitchSpelling *const *)scale, NOTES));
    /* And it is not a descending scale. */
    assert(!standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_DESCENDING,
                                                 (const PitchSpelling *const *)scale, NOTES));
    destroy_scale(scale, NOTES);
}

/* No E-sharp, B-sharp, F-flat or C-flat in the conventional spelling. */
static void test_enharmonic_white_key_spellings_are_unconventional(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, tonic, scale));
    pitch_spelling_destroy(scale[5]);
    scale[5] = spell(PITCH_LETTER_E, 1, 4); /* E-sharp in place of F */
    assert(standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING,
                                                (const PitchSpelling *const *)scale, NOTES));
    assert(!standard_tonality_chromatic_spelling_conventional(STANDARD_TONALITY_CHROMATIC_ASCENDING,
                                                              (const PitchSpelling *const *)scale, NOTES));
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

static void test_non_chromatic_scales_are_rejected(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, tonic, scale));
    pitch_spelling_destroy(scale[4]);
    scale[4] = spell(PITCH_LETTER_F, 0, 4); /* skips E */
    assert(!standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING,
                                                 (const PitchSpelling *const *)scale, NOTES));
    destroy_scale(scale, NOTES);

    const PitchSpelling *one[] = {tonic};
    assert(!standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, one, 1));
    assert(!standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, NULL, NOTES));
    assert(!standard_tonality_is_chromatic_scale((StandardTonalityChromaticDirection)99, NULL, NOTES));
    const PitchSpelling *with_null[NOTES] = {tonic, tonic, tonic, tonic, tonic, tonic, tonic,
                                             tonic, tonic, tonic, tonic, tonic, NULL};
    assert(!standard_tonality_is_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, with_null, NOTES));
    assert(!standard_tonality_chromatic_spelling_conventional(STANDARD_TONALITY_CHROMATIC_ASCENDING, with_null,
                                                              NOTES));
    pitch_spelling_destroy(tonic);
}

static void test_build_rejects_invalid_arguments(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *scale[NOTES] = {NULL};
    assert(!standard_tonality_build_chromatic_scale((StandardTonalityChromaticDirection)99, tonic, scale));
    assert(!standard_tonality_build_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, NULL, scale));
    assert(!standard_tonality_build_chromatic_scale(STANDARD_TONALITY_CHROMATIC_ASCENDING, tonic, NULL));
    assert(scale[0] == NULL);
    pitch_spelling_destroy(tonic);
}

/* SOLFEGE: raised ascending, lowered descending. */
static void test_solfege(void) {
    const char *ascending[NOTES] = {"do", "di", "re", "ri", "mi", "fa", "fi", "sol", "si", "la", "li", "ti", "do"};
    const char *descending[NOTES] = {"do", "ti", "te", "la", "le", "sol", "se", "fa", "mi", "me", "re", "ra", "do"};
    for (size_t i = 0; i < NOTES; i++) {
        assert(strcmp(standard_tonality_chromatic_scale_solfege(STANDARD_TONALITY_CHROMATIC_ASCENDING, i),
                      ascending[i]) == 0);
        assert(strcmp(standard_tonality_chromatic_scale_solfege(STANDARD_TONALITY_CHROMATIC_DESCENDING, i),
                      descending[i]) == 0);
    }
    assert(standard_tonality_chromatic_scale_solfege(STANDARD_TONALITY_CHROMATIC_ASCENDING, 13) == NULL);
    assert(standard_tonality_chromatic_scale_solfege((StandardTonalityChromaticDirection)99, 0) == NULL);
}

int main(void) {
    test_rules_reference_framework();
    test_natural_half_steps();
    test_c_ascending();
    test_c_descending();
    test_a_ascending_and_descending();
    test_flat_tonic();
    test_every_tonic();
    test_other_spelling_is_still_chromatic();
    test_enharmonic_white_key_spellings_are_unconventional();
    test_non_chromatic_scales_are_rejected();
    test_build_rejects_invalid_arguments();
    test_solfege();
    printf("test_chromatic_scale_rules (standard-tonality): ok\n");
    return 0;
}
