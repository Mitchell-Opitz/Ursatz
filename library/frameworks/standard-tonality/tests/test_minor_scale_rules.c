#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "spelling_test_util.h"

#define NOTES STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT

static void destroy_scale(PitchSpelling *scale[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(scale[i]);
    }
}

static void check_scale(StandardTonalityMinorForm form, PitchLetter tonic_letter, int tonic_accidental,
                        const PitchLetter letters[NOTES], const int accidentals[NOTES]) {
    PitchSpelling *tonic = spell(tonic_letter, tonic_accidental, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_minor_scale(form, tonic, scale));
    for (size_t i = 0; i < NOTES; i++) {
        assert(pitch_spelling_letter(scale[i]) == letters[i]);
        assert(pitch_spelling_accidental(scale[i]) == accidentals[i]);
    }
    assert(standard_tonality_is_minor_scale(form, (const PitchSpelling *const *)scale, NOTES));
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_MINOR_SCALE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_minor_scale_rule((StandardTonalityMinorScaleRule)i);
        const Constraint *constraint = standard_tonality_minor_scale_constraint((StandardTonalityMinorScaleRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_minor_scale_rule(STANDARD_TONALITY_MINOR_SCALE_RULE_COUNT) == NULL);
}

/* W-H-W-W-H-W-W; W-H-W-W-H-3H-H; W-H-W-W-W-W-H; W-W-H-W-W-H-W. */
static void test_step_patterns(void) {
    int expected[STANDARD_TONALITY_MINOR_FORM_COUNT][7] = {
        {2, 1, 2, 2, 1, 2, 2},
        {2, 1, 2, 2, 1, 3, 1},
        {2, 1, 2, 2, 2, 2, 1},
        {2, 2, 1, 2, 2, 1, 2},
    };
    for (int form = 0; form < STANDARD_TONALITY_MINOR_FORM_COUNT; form++) {
        int total = 0;
        for (size_t i = 0; i < 7; i++) {
            assert(standard_tonality_minor_scale_step((StandardTonalityMinorForm)form, i) == expected[form][i]);
            total += expected[form][i];
        }
        assert(total == 12); /* every form spans an octave */
    }
}

static void test_a_minor_forms(void) {
    PitchLetter ascending[NOTES] = {PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C, PITCH_LETTER_D,
                                    PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_A};
    PitchLetter descending[NOTES] = {PITCH_LETTER_A, PITCH_LETTER_G, PITCH_LETTER_F, PITCH_LETTER_E,
                                     PITCH_LETTER_D, PITCH_LETTER_C, PITCH_LETTER_B, PITCH_LETTER_A};
    int natural[NOTES] = {0, 0, 0, 0, 0, 0, 0, 0};
    int harmonic[NOTES] = {0, 0, 0, 0, 0, 0, 1, 0}; /* G-sharp */
    int melodic_up[NOTES] = {0, 0, 0, 0, 0, 1, 1, 0}; /* F-sharp G-sharp */

    check_scale(STANDARD_TONALITY_MINOR_NATURAL, PITCH_LETTER_A, 0, ascending, natural);
    check_scale(STANDARD_TONALITY_MINOR_HARMONIC, PITCH_LETTER_A, 0, ascending, harmonic);
    check_scale(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, PITCH_LETTER_A, 0, ascending, melodic_up);
    /* Descending melodic is natural minor, listed from the upper tonic. */
    check_scale(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, PITCH_LETTER_A, 0, descending, natural);
}

static void test_g_minor_forms(void) {
    PitchLetter ascending[NOTES] = {PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C,
                                    PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G};
    int natural[NOTES] = {0, 0, -1, 0, 0, -1, 0, 0};       /* B-flat E-flat */
    int harmonic[NOTES] = {0, 0, -1, 0, 0, -1, 1, 0};      /* ... F-sharp */
    int melodic_up[NOTES] = {0, 0, -1, 0, 0, 0, 1, 0};     /* E natural F-sharp */

    check_scale(STANDARD_TONALITY_MINOR_NATURAL, PITCH_LETTER_G, 0, ascending, natural);
    check_scale(STANDARD_TONALITY_MINOR_HARMONIC, PITCH_LETTER_G, 0, ascending, harmonic);
    check_scale(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, PITCH_LETTER_G, 0, ascending, melodic_up);
}

static void test_descending_is_listed_from_the_upper_tonic(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, tonic, scale));
    assert(pitch_spelling_octave(scale[0]) == 5); /* upper tonic first */
    assert(pitch_spelling_octave(scale[7]) == 4);
    assert(pitch_spelling_equals(scale[7], tonic));
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

/* A scale of one form is not a scale of another with a different pattern. */
static void test_forms_are_distinct(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_A, 0, 4);
    for (int built_form = 0; built_form < STANDARD_TONALITY_MINOR_FORM_COUNT; built_form++) {
        PitchSpelling *scale[NOTES];
        assert(standard_tonality_build_minor_scale((StandardTonalityMinorForm)built_form, tonic, scale));
        for (int checked = 0; checked < STANDARD_TONALITY_MINOR_FORM_COUNT; checked++) {
            bool expected = built_form == checked;
            assert(standard_tonality_is_minor_scale((StandardTonalityMinorForm)checked,
                                                    (const PitchSpelling *const *)scale, NOTES) == expected);
        }
        destroy_scale(scale, NOTES);
    }
    pitch_spelling_destroy(tonic);
}

static void test_a_major_scale_is_not_a_minor_scale(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_major_scale(tonic, scale));
    for (int form = 0; form < STANDARD_TONALITY_MINOR_FORM_COUNT; form++) {
        assert(!standard_tonality_is_minor_scale((StandardTonalityMinorForm)form,
                                                 (const PitchSpelling *const *)scale, NOTES));
    }
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

/* Wrong spelling: the harmonic minor's 3H gap must be a raised seventh
   (G-sharp), not an A-flat. */
static void test_harmonic_gap_must_be_spelled_with_the_seventh(void) {
    PitchSpelling *scale[NOTES] = {spell(PITCH_LETTER_A, 0, 4), spell(PITCH_LETTER_B, 0, 4),
                                   spell(PITCH_LETTER_C, 0, 5), spell(PITCH_LETTER_D, 0, 5),
                                   spell(PITCH_LETTER_E, 0, 5), spell(PITCH_LETTER_F, 0, 5),
                                   spell(PITCH_LETTER_A, -1, 5), spell(PITCH_LETTER_A, 0, 5)};
    assert(!standard_tonality_is_minor_scale(STANDARD_TONALITY_MINOR_HARMONIC, (const PitchSpelling *const *)scale,
                                             NOTES));
    destroy_scale(scale, NOTES);
}

static void test_invalid_arguments(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *scale[NOTES] = {NULL};
    assert(!standard_tonality_build_minor_scale((StandardTonalityMinorForm)99, tonic, scale));
    assert(!standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, NULL, scale));
    assert(scale[0] == NULL);
    assert(!standard_tonality_is_minor_scale((StandardTonalityMinorForm)99, NULL, NOTES));
    assert(!standard_tonality_is_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, NULL, NOTES));
    const PitchSpelling *one[] = {tonic};
    assert(!standard_tonality_is_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, one, 1));
    pitch_spelling_destroy(tonic);
}

/* Beyond a double accidental: the harmonic minor on G double-sharp needs
   F triple-sharp. */
static void test_build_rejects_scales_beyond_double_accidentals(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_G, 2, 4);
    PitchSpelling *scale[NOTES] = {NULL};
    assert(!standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, scale));
    for (size_t i = 0; i < NOTES; i++) {
        assert(scale[i] == NULL);
    }
    pitch_spelling_destroy(tonic);
}

/* Property: a minor scale's third is a half step lower than the major
   scale's third on the same tonic, whatever the tonic. */
static void test_third_is_lowered(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            PitchSpelling *tonic = spell(letters[l], accidental, 4);
            assert(standard_tonality_minor_third_lowered(tonic));
            pitch_spelling_destroy(tonic);
        }
    }
    assert(!standard_tonality_minor_third_lowered(NULL));

    /* Same for every minor form: only the third matters, all share it. */
    PitchSpelling *b = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *major[8];
    PitchSpelling *minor[NOTES];
    assert(standard_tonality_build_major_scale(b, major));
    assert(standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_HARMONIC, b, minor));
    assert(pitch_spelling_letter(major[2]) == pitch_spelling_letter(minor[2]));
    assert(pitch_spelling_accidental(major[2]) == 1 && pitch_spelling_accidental(minor[2]) == 0); /* D-sharp, D */
    destroy_scale(major, 8);
    destroy_scale(minor, NOTES);
    pitch_spelling_destroy(b);
}

int main(void) {
    test_rules_reference_framework();
    test_step_patterns();
    test_a_minor_forms();
    test_g_minor_forms();
    test_descending_is_listed_from_the_upper_tonic();
    test_forms_are_distinct();
    test_a_major_scale_is_not_a_minor_scale();
    test_harmonic_gap_must_be_spelled_with_the_seventh();
    test_invalid_arguments();
    test_build_rejects_scales_beyond_double_accidentals();
    test_third_is_lowered();
    printf("test_minor_scale_rules (standard-tonality): ok\n");
    return 0;
}
