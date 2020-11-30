#include <assert.h>
#include <stdio.h>

#include "circle_of_fifths_rules.h"
#include "framework.h"
#include "key_relationship_rules.h"
#include "spelling_test_util.h"

static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                       PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};

static void check_relative_minor(PitchLetter major_letter, int major_accidental, PitchLetter minor_letter,
                                 int minor_accidental) {
    PitchLetter letter = PITCH_LETTER_C;
    int accidental = 99;
    assert(standard_tonality_relative_minor(major_letter, major_accidental, &letter, &accidental));
    assert(letter == minor_letter && accidental == minor_accidental);

    assert(standard_tonality_relative_major(minor_letter, minor_accidental, &letter, &accidental));
    assert(letter == major_letter && accidental == major_accidental);

    assert(standard_tonality_keys_relative(major_letter, major_accidental, minor_letter, minor_accidental));
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_key_relationship_rule((StandardTonalityKeyRelationshipRule)i);
        const Constraint *constraint =
            standard_tonality_key_relationship_constraint((StandardTonalityKeyRelationshipRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_key_relationship_rule(STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_COUNT) == NULL);
}

/* PARALLEL: C major and C minor, A-flat major and A-flat minor, F-sharp
   major and F-sharp minor. */
static void test_parallel_keys(void) {
    assert(standard_tonality_keys_parallel(PITCH_LETTER_C, 0, PITCH_LETTER_C, 0));
    assert(standard_tonality_keys_parallel(PITCH_LETTER_A, -1, PITCH_LETTER_A, -1));
    assert(standard_tonality_keys_parallel(PITCH_LETTER_F, 1, PITCH_LETTER_F, 1));
    assert(!standard_tonality_keys_parallel(PITCH_LETTER_C, 0, PITCH_LETTER_A, 0)); /* relative, not parallel */
    assert(!standard_tonality_keys_parallel(PITCH_LETTER_C, 0, PITCH_LETTER_C, 1));
    assert(!standard_tonality_keys_parallel(PITCH_LETTER_B, 0, PITCH_LETTER_C, -1)); /* enharmonic is not parallel */
}

/* RELATIVE: C major has no sharps or flats and neither does A minor. */
static void test_relative_pairs(void) {
    check_relative_minor(PITCH_LETTER_C, 0, PITCH_LETTER_A, 0);
    check_relative_minor(PITCH_LETTER_G, 0, PITCH_LETTER_E, 0);
    check_relative_minor(PITCH_LETTER_D, 0, PITCH_LETTER_B, 0);
    check_relative_minor(PITCH_LETTER_A, 0, PITCH_LETTER_F, 1);
    check_relative_minor(PITCH_LETTER_E, 0, PITCH_LETTER_C, 1);
    check_relative_minor(PITCH_LETTER_B, 0, PITCH_LETTER_G, 1);
    check_relative_minor(PITCH_LETTER_F, 1, PITCH_LETTER_D, 1);
    check_relative_minor(PITCH_LETTER_C, 1, PITCH_LETTER_A, 1);

    check_relative_minor(PITCH_LETTER_F, 0, PITCH_LETTER_D, 0);
    check_relative_minor(PITCH_LETTER_B, -1, PITCH_LETTER_G, 0);
    check_relative_minor(PITCH_LETTER_E, -1, PITCH_LETTER_C, 0);
    check_relative_minor(PITCH_LETTER_A, -1, PITCH_LETTER_F, 0);
    check_relative_minor(PITCH_LETTER_D, -1, PITCH_LETTER_B, -1);
    check_relative_minor(PITCH_LETTER_G, -1, PITCH_LETTER_E, -1);
    check_relative_minor(PITCH_LETTER_C, -1, PITCH_LETTER_A, -1);
}

/* RELATIVE_SPELLING: D-flat major's relative minor is B-flat minor (five
   flats), not A-sharp minor (seven sharps), though both sit three half
   steps below. */
static void test_relative_spelling(void) {
    assert(standard_tonality_keys_relative(PITCH_LETTER_D, -1, PITCH_LETTER_B, -1));
    assert(!standard_tonality_keys_relative(PITCH_LETTER_D, -1, PITCH_LETTER_A, 1));

    /* Both are three half steps below, yet only one shares the signature. */
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_D, -1) == -5);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_A, 1) - 3 == 7);

    /* Not relative at all. */
    assert(!standard_tonality_keys_relative(PITCH_LETTER_C, 0, PITCH_LETTER_B, 0));
    assert(!standard_tonality_keys_relative(PITCH_LETTER_C, 0, PITCH_LETTER_C, 0));
    assert(!standard_tonality_keys_relative(PITCH_LETTER_A, 0, PITCH_LETTER_C, 0)); /* the wrong way round */
}

/* Property: for every major key with a scale, the relative minor sounds
   three half steps below, and going back returns the same major key. */
static void test_relative_round_trip(void) {
    int pairs = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            PitchLetter minor_letter;
            int minor_accidental;
            assert(standard_tonality_relative_minor(LETTERS[l], accidental, &minor_letter, &minor_accidental));
            assert(standard_tonality_keys_relative(LETTERS[l], accidental, minor_letter, minor_accidental));

            PitchLetter back_letter;
            int back_accidental;
            assert(standard_tonality_relative_major(minor_letter, minor_accidental, &back_letter, &back_accidental));
            assert(back_letter == LETTERS[l] && back_accidental == accidental);
            pairs++;
        }
    }
    assert(pairs == 21);
}

/* Relative keys have the same signature: the same accidental count,
   whichever key is imaginary. */
static void test_relative_keys_share_a_signature_count(void) {
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            PitchLetter minor_letter;
            int minor_accidental;
            assert(standard_tonality_relative_minor(LETTERS[l], accidental, &minor_letter, &minor_accidental));
            assert(standard_tonality_major_key_accidentals(minor_letter, minor_accidental) - 3 ==
                   standard_tonality_major_key_accidentals(LETTERS[l], accidental));
        }
    }
}

static void test_invalid_arguments(void) {
    PitchLetter letter = PITCH_LETTER_D;
    int accidental = 42;

    /* G double-sharp major needs F triple-sharp. */
    assert(!standard_tonality_relative_minor(PITCH_LETTER_G, 2, &letter, &accidental));
    assert(letter == PITCH_LETTER_D && accidental == 42);
    assert(!standard_tonality_keys_relative(PITCH_LETTER_G, 2, PITCH_LETTER_E, 2));

    assert(!standard_tonality_relative_minor(PITCH_LETTER_C, 0, NULL, &accidental));
    assert(!standard_tonality_relative_minor(PITCH_LETTER_C, 0, &letter, NULL));
    assert(!standard_tonality_relative_major(PITCH_LETTER_A, 0, NULL, &accidental));
    assert(!standard_tonality_relative_major(PITCH_LETTER_A, 0, &letter, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_parallel_keys();
    test_relative_pairs();
    test_relative_spelling();
    test_relative_round_trip();
    test_relative_keys_share_a_signature_count();
    test_invalid_arguments();
    printf("test_key_relationship_rules (standard-tonality): ok\n");
    return 0;
}
