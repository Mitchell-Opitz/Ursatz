#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "major_scale_rules.h"
#include "spelling_test_util.h"

static void destroy_scale(PitchSpelling *scale[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(scale[i]);
    }
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_major_scale_rule((StandardTonalityMajorScaleRule)i);
        const Constraint *constraint = standard_tonality_major_scale_constraint((StandardTonalityMajorScaleRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_major_scale_rule(STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT) == NULL);
}

/* W W H W W W H. */
static void test_step_pattern(void) {
    int expected[] = {2, 2, 1, 2, 2, 2, 1};
    int total = 0;
    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT; i++) {
        assert(standard_tonality_major_scale_step(i) == expected[i]);
        total += standard_tonality_major_scale_step(i);
    }
    assert(total == 12);
}

static void test_c_major_is_a_major_scale(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C};
    int octaves[] = {4, 4, 4, 4, 4, 4, 4, 5};
    PitchSpelling *scale[8];
    for (size_t i = 0; i < 8; i++) {
        scale[i] = spell(letters[i], 0, octaves[i]);
    }
    assert(standard_tonality_is_major_scale((const PitchSpelling *const *)scale, 8));
    assert(standard_tonality_scale_closes_on_tonic(scale[0], scale[7]));
    destroy_scale(scale, 8);
}

static void test_b_flat_major_is_a_major_scale(void) {
    /* B-flat C D E-flat F G A B-flat: the octave belongs to the letter. */
    PitchSpelling *scale[8] = {spell(PITCH_LETTER_B, -1, 3), spell(PITCH_LETTER_C, 0, 4),
                               spell(PITCH_LETTER_D, 0, 4),  spell(PITCH_LETTER_E, -1, 4),
                               spell(PITCH_LETTER_F, 0, 4),  spell(PITCH_LETTER_G, 0, 4),
                               spell(PITCH_LETTER_A, 0, 4),  spell(PITCH_LETTER_B, -1, 4)};
    assert(standard_tonality_is_major_scale((const PitchSpelling *const *)scale, 8));
    assert(standard_tonality_scale_closes_on_tonic(scale[0], scale[7]));
    destroy_scale(scale, 8);
}

/* B major (five sharps) is a different scale from B-flat major. */
static void test_b_major_is_not_b_flat_major(void) {
    PitchSpelling *b_flat = spell(PITCH_LETTER_B, -1, 3);
    PitchSpelling *b = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *b_next_octave = spell(PITCH_LETTER_B, 0, 4);
    assert(!standard_tonality_same_scale_name(b_flat, b));
    assert(standard_tonality_same_scale_name(b, b_next_octave)); /* octave does not name a scale */
    assert(!standard_tonality_same_scale_name(b, NULL));
    pitch_spelling_destroy(b_flat);
    pitch_spelling_destroy(b);
    pitch_spelling_destroy(b_next_octave);
}

static void test_wrong_pattern_is_rejected(void) {
    /* C D E-flat F G A B C: a half step too soon after D. */
    PitchSpelling *scale[8] = {spell(PITCH_LETTER_C, 0, 4), spell(PITCH_LETTER_D, 0, 4),
                               spell(PITCH_LETTER_E, -1, 4), spell(PITCH_LETTER_F, 0, 4),
                               spell(PITCH_LETTER_G, 0, 4),  spell(PITCH_LETTER_A, 0, 4),
                               spell(PITCH_LETTER_B, 0, 4),  spell(PITCH_LETTER_C, 0, 5)};
    assert(!standard_tonality_is_major_scale((const PitchSpelling *const *)scale, 8));
    destroy_scale(scale, 8);
}

/* Right sounds, wrong spelling: C major with E-sharp where F belongs. */
static void test_wrong_spelling_is_rejected(void) {
    PitchSpelling *scale[8] = {spell(PITCH_LETTER_C, 0, 4), spell(PITCH_LETTER_D, 0, 4),
                               spell(PITCH_LETTER_E, 0, 4), spell(PITCH_LETTER_F, 0, 4),
                               spell(PITCH_LETTER_G, 0, 4), spell(PITCH_LETTER_A, 0, 4),
                               spell(PITCH_LETTER_B, 0, 4), spell(PITCH_LETTER_C, 0, 5)};
    /* E-sharp sounds like F, but E would be used twice and F skipped. */
    pitch_spelling_destroy(scale[3]);
    scale[3] = spell(PITCH_LETTER_E, 1, 4);
    assert(!standard_tonality_is_major_scale((const PitchSpelling *const *)scale, 8));
    destroy_scale(scale, 8);
}

static void test_wrong_count_and_null_are_rejected(void) {
    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    const PitchSpelling *one[] = {c};
    assert(!standard_tonality_is_major_scale(one, 1));
    assert(!standard_tonality_is_major_scale(NULL, 8));
    const PitchSpelling *with_null[8] = {c, c, c, c, c, c, c, NULL};
    assert(!standard_tonality_is_major_scale(with_null, 8));
    pitch_spelling_destroy(c);
}

static void test_closes_on_tonic(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *c6 = spell(PITCH_LETTER_C, 0, 6);
    PitchSpelling *c_sharp5 = spell(PITCH_LETTER_C, 1, 5);
    PitchSpelling *d5 = spell(PITCH_LETTER_D, 0, 5);
    assert(standard_tonality_scale_closes_on_tonic(c4, c5));
    assert(!standard_tonality_scale_closes_on_tonic(c4, c6)); /* two octaves */
    assert(!standard_tonality_scale_closes_on_tonic(c4, c4));
    assert(!standard_tonality_scale_closes_on_tonic(c4, c_sharp5));
    assert(!standard_tonality_scale_closes_on_tonic(c4, d5));
    assert(!standard_tonality_scale_closes_on_tonic(c4, NULL));
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(c6);
    pitch_spelling_destroy(c_sharp5);
    pitch_spelling_destroy(d5);
}

/* Property: the scale built on every tonic is a major scale and closes
   on its tonic. */
static void test_built_scales_are_major_scales(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            PitchSpelling *tonic = spell(letters[l], accidental, 4);
            PitchSpelling *scale[8];
            assert(standard_tonality_build_major_scale(tonic, scale));
            assert(standard_tonality_is_major_scale((const PitchSpelling *const *)scale, 8));
            assert(standard_tonality_scale_closes_on_tonic(scale[0], scale[7]));
            assert(pitch_spelling_equals(scale[0], tonic));
            destroy_scale(scale, 8);
            pitch_spelling_destroy(tonic);
        }
    }
}

static void test_built_f_sharp_major(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_F, 1, 4);
    PitchSpelling *scale[8];
    assert(standard_tonality_build_major_scale(tonic, scale));
    PitchLetter letters[] = {PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B,
                             PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F};
    int accidentals[] = {1, 1, 1, 0, 1, 1, 1, 1}; /* E is E-sharp */
    for (size_t i = 0; i < 8; i++) {
        assert(pitch_spelling_letter(scale[i]) == letters[i]);
        assert(pitch_spelling_accidental(scale[i]) == accidentals[i]);
    }
    destroy_scale(scale, 8);
    pitch_spelling_destroy(tonic);
}

static void test_built_f_flat_major_needs_a_double_flat(void) {
    /* F-flat major: F-flat G-flat A-flat B-double-flat C-flat D-flat E-flat F-flat. */
    PitchSpelling *tonic = spell(PITCH_LETTER_F, -1, 4);
    PitchSpelling *scale[8];
    assert(standard_tonality_build_major_scale(tonic, scale));
    assert(pitch_spelling_letter(scale[3]) == PITCH_LETTER_B);
    assert(pitch_spelling_accidental(scale[3]) == -2);
    destroy_scale(scale, 8);
    pitch_spelling_destroy(tonic);
}

static void test_build_rejects_scales_beyond_double_accidentals(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_G, 2, 4); /* G double-sharp needs F triple-sharp */
    PitchSpelling *scale[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    assert(!standard_tonality_build_major_scale(tonic, scale));
    for (size_t i = 0; i < 8; i++) {
        assert(scale[i] == NULL); /* untouched */
    }
    assert(!standard_tonality_build_major_scale(NULL, scale));
    pitch_spelling_destroy(tonic);
}

int main(void) {
    test_rules_reference_framework();
    test_step_pattern();
    test_c_major_is_a_major_scale();
    test_b_flat_major_is_a_major_scale();
    test_b_major_is_not_b_flat_major();
    test_wrong_pattern_is_rejected();
    test_wrong_spelling_is_rejected();
    test_wrong_count_and_null_are_rejected();
    test_closes_on_tonic();
    test_built_scales_are_major_scales();
    test_built_f_sharp_major();
    test_built_f_flat_major_needs_a_double_flat();
    test_build_rejects_scales_beyond_double_accidentals();
    printf("test_major_scale_rules (standard-tonality): ok\n");
    return 0;
}
