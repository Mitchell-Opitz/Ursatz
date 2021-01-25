#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "interval_size_rules.h"
#include "spelling_test_util.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_interval_size_rule((StandardTonalityIntervalSizeRule)i);
        const Constraint *constraint = standard_tonality_interval_size_constraint((StandardTonalityIntervalSizeRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_interval_size_rule(STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT) == NULL);
}

/* NAMES: ordinals, except unison for 1 and octave for 8. */
static void test_names(void) {
    const char *expected[] = {"unison", "second", "third",  "fourth",   "fifth",   "sixth",   "seventh",
                              "octave", "ninth",  "tenth", "eleventh", "twelfth", "thirteenth"};
    for (int size = 1; size <= 13; size++) {
        assert(strcmp(standard_tonality_interval_size_name(size), expected[size - 1]) == 0);
    }
    assert(standard_tonality_interval_size_name(0) == NULL);
    assert(standard_tonality_interval_size_name(14) == NULL);
    assert(standard_tonality_interval_size_name(-1) == NULL);
}

/* COUNTING: Example 2 -- begin with one. */
static void test_counting(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C};
    int octaves[] = {4, 4, 4, 4, 4, 4, 4, 5};
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    for (int i = 0; i < 8; i++) {
        PitchSpelling *upper = spell(letters[i], 0, octaves[i]);
        int size = 0;
        assert(standard_tonality_interval_generic_size(c4, upper, &size));
        assert(size == i + 1);
        pitch_spelling_destroy(upper);
    }
    pitch_spelling_destroy(c4);
}

/* Example 5: F to C counts F G A B C: a fifth. E-flat to C-flat counts to
   six. */
static void test_counting_examples(void) {
    int size = 0;
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    assert(standard_tonality_interval_generic_size(f4, c5, &size) && size == 5);

    PitchSpelling *e_flat4 = spell(PITCH_LETTER_E, -1, 4);
    PitchSpelling *c_flat5 = spell(PITCH_LETTER_C, -1, 5);
    assert(standard_tonality_interval_generic_size(e_flat4, c_flat5, &size) && size == 6);

    pitch_spelling_destroy(f4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(e_flat4);
    pitch_spelling_destroy(c_flat5);
}

/* GENERIC: Example 3 -- every accidental on either note leaves a third. */
static void test_size_ignores_accidentals(void) {
    for (int lower_accidental = -2; lower_accidental <= 2; lower_accidental++) {
        for (int upper_accidental = -2; upper_accidental <= 2; upper_accidental++) {
            PitchSpelling *lower = spell(PITCH_LETTER_C, lower_accidental, 4);
            PitchSpelling *upper = spell(PITCH_LETTER_E, upper_accidental, 4);
            int size = 0;
            assert(standard_tonality_interval_generic_size(lower, upper, &size));
            assert(size == 3);
            pitch_spelling_destroy(lower);
            pitch_spelling_destroy(upper);
        }
    }
}

static void test_generic_size_rejects_invalid_arguments(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    int size = 42;
    assert(!standard_tonality_interval_generic_size(c4, b3, &size)); /* upper below lower */
    assert(!standard_tonality_interval_generic_size(NULL, c4, &size));
    assert(!standard_tonality_interval_generic_size(c4, NULL, &size));
    assert(!standard_tonality_interval_generic_size(c4, c4, NULL));
    assert(size == 42);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(b3);
}

/* COMPOUND: simple is 1..8, compound larger; add 7. */
static void test_simple_and_compound(void) {
    for (int size = 1; size <= 8; size++) {
        assert(standard_tonality_interval_is_simple(size));
        assert(!standard_tonality_interval_is_compound(size));
        assert(standard_tonality_interval_simple_size(size) == size);
    }
    for (int size = 9; size <= 22; size++) {
        assert(!standard_tonality_interval_is_simple(size));
        assert(standard_tonality_interval_is_compound(size));
    }
    assert(!standard_tonality_interval_is_simple(0));
    assert(!standard_tonality_interval_is_compound(0));
    assert(standard_tonality_interval_simple_size(0) == 0);
    assert(standard_tonality_interval_simple_size(-5) == 0);
}

/* Unisons become octaves, 2nds 9ths, 3rds 10ths, 4ths 11ths, 5ths 12ths,
   6ths 13ths. */
static void test_compound_sizes(void) {
    int expected[] = {8, 9, 10, 11, 12, 13};
    for (int size = 1; size <= 6; size++) {
        assert(standard_tonality_interval_compound_size(size) == expected[size - 1]);
        assert(standard_tonality_interval_simple_size(expected[size - 1]) == (size == 1 ? 8 : size));
    }
    assert(standard_tonality_interval_compound_size(0) == 0);
    assert(standard_tonality_interval_compound_size(9) == 0);
    assert(standard_tonality_interval_simple_size(10) == 3);
    assert(standard_tonality_interval_simple_size(15) == 8);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_counting();
    test_counting_examples();
    test_size_ignores_accidentals();
    test_generic_size_rejects_invalid_arguments();
    test_simple_and_compound();
    test_compound_sizes();
    printf("test_interval_size_rules (standard-tonality): ok\n");
    return 0;
}
