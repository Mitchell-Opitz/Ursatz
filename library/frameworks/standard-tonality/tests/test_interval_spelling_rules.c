#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "interval_spelling_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalityIntervalQuality Quality;

#define P STANDARD_TONALITY_QUALITY_PERFECT
#define MA STANDARD_TONALITY_QUALITY_MAJOR
#define MI STANDARD_TONALITY_QUALITY_MINOR
#define AUG STANDARD_TONALITY_QUALITY_AUGMENTED
#define DIM STANDARD_TONALITY_QUALITY_DIMINISHED

static void check(PitchLetter lower_letter, int lower_accidental, int lower_octave, int size, Quality quality,
                  PitchLetter expected_letter, int expected_accidental, int expected_octave) {
    PitchSpelling *lower = spell(lower_letter, lower_accidental, lower_octave);
    PitchSpelling *upper = NULL;
    assert(standard_tonality_spell_above(lower, size, quality, &upper));
    assert(pitch_spelling_letter(upper) == expected_letter);
    assert(pitch_spelling_accidental(upper) == expected_accidental);
    assert(pitch_spelling_octave(upper) == expected_octave);
    pitch_spelling_destroy(lower);
    pitch_spelling_destroy(upper);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_INTERVAL_SPELLING_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_interval_spelling_rule((StandardTonalityIntervalSpellingRule)i);
        const Constraint *constraint =
            standard_tonality_interval_spelling_constraint((StandardTonalityIntervalSpellingRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_interval_spelling_rule(STANDARD_TONALITY_INTERVAL_SPELLING_RULE_COUNT) == NULL);
}

/* Examples 5 to 7 read in reverse: given the bottom note, size, and quality. */
static void test_source_intervals(void) {
    check(PITCH_LETTER_F, 0, 4, 5, P, PITCH_LETTER_C, 0, 5);    /* P5 above F is C */
    check(PITCH_LETTER_E, -1, 4, 6, MI, PITCH_LETTER_C, -1, 5); /* mi6 above E-flat is C-flat */
    check(PITCH_LETTER_E, -1, 4, 6, MA, PITCH_LETTER_C, 0, 5);  /* ma6 is C natural */
    check(PITCH_LETTER_F, 0, 4, 5, AUG, PITCH_LETTER_C, 1, 5);  /* A5 above F is C-sharp */
    check(PITCH_LETTER_G, 0, 4, 6, MA, PITCH_LETTER_E, 0, 5);
    check(PITCH_LETTER_G, 0, 4, 6, AUG, PITCH_LETTER_E, 1, 5);  /* A6 above G is E-sharp */
    check(PITCH_LETTER_F, 0, 4, 5, DIM, PITCH_LETTER_C, -1, 5); /* d5 above F is C-flat */
    check(PITCH_LETTER_G, 0, 4, 6, MI, PITCH_LETTER_E, -1, 5);
    check(PITCH_LETTER_G, 0, 4, 6, DIM, PITCH_LETTER_E, -2, 5); /* d6: E double-flat */
}

/* Example 9: a simple and a compound interval above A2. */
static void test_compound(void) {
    check(PITCH_LETTER_A, 0, 2, 3, MI, PITCH_LETTER_C, 0, 3);
    check(PITCH_LETTER_A, 0, 2, 10, MI, PITCH_LETTER_C, 0, 4);
    check(PITCH_LETTER_C, 0, 4, 8, P, PITCH_LETTER_C, 0, 5);
    check(PITCH_LETTER_C, 0, 4, 15, P, PITCH_LETTER_C, 0, 6);
}

static void test_unison_and_octave(void) {
    check(PITCH_LETTER_D, 0, 4, 1, P, PITCH_LETTER_D, 0, 4);
    check(PITCH_LETTER_D, 0, 4, 1, AUG, PITCH_LETTER_D, 1, 4);
    check(PITCH_LETTER_D, 0, 4, 8, P, PITCH_LETTER_D, 0, 5);
}

/* The upper note spells so that the interval is identified as asked. */
static void test_agrees_with_identification(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int spelled = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            PitchSpelling *lower = spell(letters[l], accidental, 3);
            for (int size = 1; size <= 15; size++) {
                for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
                    PitchSpelling *upper = NULL;
                    if (!standard_tonality_spell_above(lower, size, (Quality)q, &upper)) {
                        assert(upper == NULL);
                        continue;
                    }
                    int found_size = 0;
                    Quality found_quality = P;
                    /* A diminished unison would be a negative interval; the
                       identification rules reject it. */
                    if (standard_tonality_interval_quality(lower, upper, &found_size, &found_quality)) {
                        assert(found_size == size);
                        assert(found_quality == (Quality)q);
                        spelled++;
                    }
                    pitch_spelling_destroy(upper);
                }
            }
            pitch_spelling_destroy(lower);
        }
    }
    assert(spelled > 500);
}

static void test_rejects_invalid_arguments(void) {
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *upper = NULL;
    assert(!standard_tonality_spell_above(f4, 0, P, &upper));
    assert(!standard_tonality_spell_above(f4, -2, P, &upper));
    assert(!standard_tonality_spell_above(f4, 3, P, &upper)); /* no perfect third */
    assert(!standard_tonality_spell_above(f4, 5, MA, &upper)); /* no major fifth */
    assert(!standard_tonality_spell_above(f4, 5, (Quality)99, &upper));
    assert(!standard_tonality_spell_above(NULL, 5, P, &upper));
    assert(!standard_tonality_spell_above(f4, 5, P, NULL));
    assert(upper == NULL);
    pitch_spelling_destroy(f4);

    /* An augmented fifth above G double-sharp needs D triple-sharp. */
    PitchSpelling *g_double_sharp = spell(PITCH_LETTER_G, 2, 4);
    assert(!standard_tonality_spell_above(g_double_sharp, 5, AUG, &upper));
    assert(upper == NULL);
    pitch_spelling_destroy(g_double_sharp);
}

int main(void) {
    test_rules_reference_framework();
    test_source_intervals();
    test_compound();
    test_unison_and_octave();
    test_agrees_with_identification();
    test_rejects_invalid_arguments();
    printf("test_interval_spelling_rules (standard-tonality): ok\n");
    return 0;
}
