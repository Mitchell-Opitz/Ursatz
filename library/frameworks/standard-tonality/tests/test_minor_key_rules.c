#include <assert.h>
#include <stdio.h>

#include "circle_of_fifths_rules.h"
#include "framework.h"
#include "key_relationship_rules.h"
#include "key_signature_rules.h"
#include "minor_key_rules.h"
#include "spelling_test_util.h"

static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                       PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
static const PitchLetter SHARPS[7] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                                      PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
static const PitchLetter FLATS[7] = {PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A, PITCH_LETTER_D,
                                     PITCH_LETTER_G, PITCH_LETTER_C, PITCH_LETTER_F};

static KeySignature *make_signature(const KeySignatureEntry *entries, size_t count) {
    KeySignature *key_signature = NULL;
    assert(key_signature_create(entries, count, &key_signature) == KEY_SIGNATURE_OK);
    return key_signature;
}

static KeySignature *make_sharps(size_t count) {
    KeySignatureEntry entries[7];
    for (size_t i = 0; i < count; i++) {
        entries[i].letter = SHARPS[i];
        entries[i].accidental = 1;
    }
    return make_signature(entries, count);
}

static KeySignature *make_flats(size_t count) {
    KeySignatureEntry entries[7];
    for (size_t i = 0; i < count; i++) {
        entries[i].letter = FLATS[i];
        entries[i].accidental = -1;
    }
    return make_signature(entries, count);
}

static void check_minor_key(const KeySignature *key_signature, PitchLetter expected_letter, int expected_accidental) {
    PitchLetter letter = PITCH_LETTER_C;
    int accidental = 99;
    assert(standard_tonality_minor_key_of_signature(key_signature, &letter, &accidental));
    assert(letter == expected_letter);
    assert(accidental == expected_accidental);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_MINOR_KEY_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_minor_key_rule((StandardTonalityMinorKeyRule)i);
        const Constraint *constraint = standard_tonality_minor_key_constraint((StandardTonalityMinorKeyRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_minor_key_rule(STANDARD_TONALITY_MINOR_KEY_RULE_COUNT) == NULL);
}

/* NATURAL_BASIS: a minor key rests on natural minor, not harmonic or
   melodic. */
static void test_minor_keys_rest_on_natural_minor(void) {
    assert(standard_tonality_minor_key_scale_form() == STANDARD_TONALITY_MINOR_NATURAL);
}

/* SIGNATURE_TONIC, Example 12: A E B F-sharp C-sharp G-sharp D-sharp
   A-sharp minor. */
static void test_sharp_minor_signatures(void) {
    PitchLetter letters[] = {PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B, PITCH_LETTER_F,
                             PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D, PITCH_LETTER_A};
    int accidentals[] = {0, 0, 0, 1, 1, 1, 1, 1};
    for (size_t count = 0; count <= 7; count++) {
        KeySignature *key_signature = make_sharps(count);
        check_minor_key(key_signature, letters[count], accidentals[count]);
        key_signature_destroy(key_signature);
    }
}

/* Example 13: A D G C F B-flat E-flat A-flat minor. */
static void test_flat_minor_signatures(void) {
    PitchLetter letters[] = {PITCH_LETTER_A, PITCH_LETTER_D, PITCH_LETTER_G, PITCH_LETTER_C,
                             PITCH_LETTER_F, PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A};
    int accidentals[] = {0, 0, 0, 0, 0, -1, -1, -1};
    for (size_t count = 0; count <= 7; count++) {
        KeySignature *key_signature = make_flats(count);
        check_minor_key(key_signature, letters[count], accidentals[count]);
        key_signature_destroy(key_signature);
    }
}

static void test_unconventional_signatures_have_no_minor_key(void) {
    KeySignatureEntry swapped[] = {{PITCH_LETTER_C, 1}, {PITCH_LETTER_F, 1}};
    KeySignature *key_signature = make_signature(swapped, 2);
    PitchLetter letter = PITCH_LETTER_D;
    int accidental = 42;
    assert(!standard_tonality_minor_key_of_signature(key_signature, &letter, &accidental));
    assert(letter == PITCH_LETTER_D && accidental == 42);
    assert(!standard_tonality_minor_key_of_signature(NULL, &letter, &accidental));
    assert(!standard_tonality_minor_key_of_signature(key_signature, NULL, &accidental));
    assert(!standard_tonality_minor_key_of_signature(key_signature, &letter, NULL));
    key_signature_destroy(key_signature);
}

/* Accidental counts match the signatures above, and relative keys agree. */
static void test_accidental_counts(void) {
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_A, 0) == 0);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_E, 0) == 1);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_B, 0) == 2);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_F, 1) == 3);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_C, 1) == 4);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_G, 1) == 5);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_D, 1) == 6);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_A, 1) == 7);

    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_D, 0) == -1);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_G, 0) == -2);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_C, 0) == -3);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_F, 0) == -4);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_B, -1) == -5);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_E, -1) == -6);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_A, -1) == -7);
}

/* REAL_IMAGINARY: A-sharp minor has seven sharps (real); the minor key a
   half step lower than D-flat major's relative, A-sharp's neighbor
   D-flat minor, would need eight flats. */
static void test_real_and_imaginary_minor_keys(void) {
    int real = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            if (standard_tonality_minor_key_is_real(LETTERS[l], accidental)) {
                real++;
            }
        }
    }
    assert(real == 15);

    assert(standard_tonality_minor_key_is_real(PITCH_LETTER_A, 1));
    assert(standard_tonality_minor_key_is_real(PITCH_LETTER_A, -1));
    assert(!standard_tonality_minor_key_is_real(PITCH_LETTER_D, -1)); /* D-flat minor: 8 flats */
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_D, -1) == -8);
    assert(!standard_tonality_minor_key_is_real(PITCH_LETTER_E, 1));  /* E-sharp minor: 8 sharps */
    assert(!standard_tonality_minor_key_is_real(PITCH_LETTER_A, 2));
}

/* Property: the signature built for a real minor key is conventional and
   belongs back to that key. */
static void test_signature_round_trips_to_minor_key(void) {
    int round_trips = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            KeySignature *key_signature = NULL;
            if (!standard_tonality_minor_key_is_real(LETTERS[l], accidental)) {
                assert(!standard_tonality_minor_key_signature(LETTERS[l], accidental, &key_signature));
                assert(key_signature == NULL);
                continue;
            }
            assert(standard_tonality_minor_key_signature(LETTERS[l], accidental, &key_signature));
            assert(standard_tonality_is_conventional_signature(key_signature));
            check_minor_key(key_signature, LETTERS[l], accidental);
            round_trips++;
            key_signature_destroy(key_signature);
        }
    }
    assert(round_trips == 15);
}

/* A relative pair has one signature: the minor key's signature is its
   relative major's. */
static void test_minor_signature_equals_relative_major_signature(void) {
    PitchLetter minor_letter;
    int minor_accidental;
    assert(standard_tonality_relative_minor(PITCH_LETTER_E, -1, &minor_letter, &minor_accidental));

    KeySignature *major_signature = NULL;
    KeySignature *minor_signature = NULL;
    assert(standard_tonality_major_key_signature(PITCH_LETTER_E, -1, &major_signature));
    assert(standard_tonality_minor_key_signature(minor_letter, minor_accidental, &minor_signature));
    assert(key_signature_equals(major_signature, minor_signature));
    key_signature_destroy(major_signature);
    key_signature_destroy(minor_signature);

    /* D-flat major and B-flat minor share five flats; A-sharp minor has
       seven sharps, a different signature. */
    assert(standard_tonality_major_key_signature(PITCH_LETTER_D, -1, &major_signature));
    assert(standard_tonality_minor_key_signature(PITCH_LETTER_B, -1, &minor_signature));
    KeySignature *a_sharp = NULL;
    assert(standard_tonality_minor_key_signature(PITCH_LETTER_A, 1, &a_sharp));
    assert(key_signature_equals(major_signature, minor_signature));
    assert(!key_signature_equals(major_signature, a_sharp));
    key_signature_destroy(major_signature);
    key_signature_destroy(minor_signature);
    key_signature_destroy(a_sharp);
}

/* CANDIDATES: a signature narrows a piece to a major key and its
   relative minor. Example 15: four flats is A-flat major or F minor. */
static void test_signature_key_candidates(void) {
    PitchLetter major_letter;
    int major_accidental;
    PitchLetter minor_letter;
    int minor_accidental;

    KeySignature *four_flats = make_flats(4);
    assert(standard_tonality_signature_key_candidates(four_flats, &major_letter, &major_accidental, &minor_letter,
                                                      &minor_accidental));
    assert(major_letter == PITCH_LETTER_A && major_accidental == -1);
    assert(minor_letter == PITCH_LETTER_F && minor_accidental == 0);
    key_signature_destroy(four_flats);

    KeySignature *none = make_sharps(0);
    assert(standard_tonality_signature_key_candidates(none, &major_letter, &major_accidental, &minor_letter,
                                                      &minor_accidental));
    assert(major_letter == PITCH_LETTER_C && major_accidental == 0);
    assert(minor_letter == PITCH_LETTER_A && minor_accidental == 0);

    KeySignature *three_sharps = make_sharps(3);
    assert(standard_tonality_signature_key_candidates(three_sharps, &major_letter, &major_accidental, &minor_letter,
                                                      &minor_accidental));
    assert(major_letter == PITCH_LETTER_A && major_accidental == 0);
    assert(minor_letter == PITCH_LETTER_F && minor_accidental == 1);
    key_signature_destroy(three_sharps);

    /* The two candidates are relative keys. */
    assert(standard_tonality_keys_relative(major_letter, major_accidental, minor_letter, minor_accidental));

    PitchLetter untouched = PITCH_LETTER_D;
    int untouched_accidental = 42;
    KeySignatureEntry swapped[] = {{PITCH_LETTER_C, 1}, {PITCH_LETTER_F, 1}};
    KeySignature *bad = make_signature(swapped, 2);
    assert(!standard_tonality_signature_key_candidates(bad, &untouched, &untouched_accidental, &minor_letter,
                                                       &minor_accidental));
    assert(untouched == PITCH_LETTER_D && untouched_accidental == 42);
    assert(!standard_tonality_signature_key_candidates(NULL, &major_letter, &major_accidental, &minor_letter,
                                                       &minor_accidental));
    assert(!standard_tonality_signature_key_candidates(none, NULL, &major_accidental, &minor_letter,
                                                       &minor_accidental));
    assert(!standard_tonality_signature_key_candidates(none, &major_letter, NULL, &minor_letter, &minor_accidental));
    assert(!standard_tonality_signature_key_candidates(none, &major_letter, &major_accidental, NULL,
                                                       &minor_accidental));
    assert(!standard_tonality_signature_key_candidates(none, &major_letter, &major_accidental, &minor_letter, NULL));
    key_signature_destroy(bad);
    key_signature_destroy(none);
}

int main(void) {
    test_rules_reference_framework();
    test_minor_keys_rest_on_natural_minor();
    test_sharp_minor_signatures();
    test_flat_minor_signatures();
    test_unconventional_signatures_have_no_minor_key();
    test_accidental_counts();
    test_real_and_imaginary_minor_keys();
    test_signature_round_trips_to_minor_key();
    test_minor_signature_equals_relative_major_signature();
    test_signature_key_candidates();
    printf("test_minor_key_rules (standard-tonality): ok\n");
    return 0;
}
