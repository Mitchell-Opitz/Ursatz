#include <assert.h>
#include <stdio.h>

#include "circle_of_fifths_rules.h"
#include "framework.h"
#include "key_relationship_rules.h"
#include "key_signature_rules.h"
#include "minor_key_rules.h"
#include "spelling_test_util.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_CIRCLE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_circle_rule((StandardTonalityCircleRule)i);
        const Constraint *constraint = standard_tonality_circle_constraint((StandardTonalityCircleRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_circle_rule(STANDARD_TONALITY_CIRCLE_RULE_COUNT) == NULL);
}

/* ORDER: sharps clockwise from C, flats counter-clockwise. */
static void test_accidental_counts(void) {
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_C, 0) == 0);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_G, 0) == 1);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_D, 0) == 2);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_A, 0) == 3);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_E, 0) == 4);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_B, 0) == 5);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_F, 1) == 6);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_C, 1) == 7);

    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_F, 0) == -1);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_B, -1) == -2);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_E, -1) == -3);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_A, -1) == -4);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_D, -1) == -5);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_G, -1) == -6);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_C, -1) == -7);
}

/* NEIGHBORS: walking clockwise from C adds one sharp each step, each key
   a fifth above the last; counter-clockwise adds a flat. */
static void test_neighbors_clockwise(void) {
    PitchLetter letters[] = {PITCH_LETTER_G, PITCH_LETTER_D, PITCH_LETTER_A, PITCH_LETTER_E,
                             PITCH_LETTER_B, PITCH_LETTER_F, PITCH_LETTER_C};
    int accidentals[] = {0, 0, 0, 0, 0, 1, 1};

    PitchLetter letter = PITCH_LETTER_C;
    int accidental = 0;
    for (int step = 1; step <= 7; step++) {
        PitchLetter next_letter;
        int next_accidental;
        assert(standard_tonality_circle_neighbor(letter, accidental, true, &next_letter, &next_accidental));
        assert(next_letter == letters[step - 1]);
        assert(next_accidental == accidentals[step - 1]);
        assert(standard_tonality_major_key_accidentals(next_letter, next_accidental) == step);
        letter = next_letter;
        accidental = next_accidental;
    }
}

static void test_neighbors_counter_clockwise(void) {
    PitchLetter letters[] = {PITCH_LETTER_F, PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A,
                             PITCH_LETTER_D, PITCH_LETTER_G, PITCH_LETTER_C};
    int accidentals[] = {0, -1, -1, -1, -1, -1, -1};

    PitchLetter letter = PITCH_LETTER_C;
    int accidental = 0;
    for (int step = 1; step <= 7; step++) {
        PitchLetter next_letter;
        int next_accidental;
        assert(standard_tonality_circle_neighbor(letter, accidental, false, &next_letter, &next_accidental));
        assert(next_letter == letters[step - 1]);
        assert(next_accidental == accidentals[step - 1]);
        assert(standard_tonality_major_key_accidentals(next_letter, next_accidental) == -step);
        letter = next_letter;
        accidental = next_accidental;
    }
}

/* Property: the two directions undo each other, and each step changes
   the accidental count by exactly one. */
static void test_neighbors_are_inverse(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            PitchLetter up_letter;
            int up_accidental;
            assert(standard_tonality_circle_neighbor(letters[l], accidental, true, &up_letter, &up_accidental));
            assert(standard_tonality_major_key_accidentals(up_letter, up_accidental) ==
                   standard_tonality_major_key_accidentals(letters[l], accidental) + 1);

            PitchLetter back_letter;
            int back_accidental;
            assert(standard_tonality_circle_neighbor(up_letter, up_accidental, false, &back_letter, &back_accidental));
            assert(back_letter == letters[l] && back_accidental == accidental);
        }
    }
}

static void test_neighbor_rejects_beyond_double_accidentals(void) {
    PitchLetter letter = PITCH_LETTER_C;
    int accidental = 0;
    /* A fifth above B double-sharp is F triple-sharp. */
    assert(!standard_tonality_circle_neighbor(PITCH_LETTER_B, 2, true, &letter, &accidental));
    assert(letter == PITCH_LETTER_C && accidental == 0);
    assert(!standard_tonality_circle_neighbor(PITCH_LETTER_C, 0, true, NULL, &accidental));
    assert(!standard_tonality_circle_neighbor(PITCH_LETTER_C, 0, true, &letter, NULL));
}

/* ENHARMONIC: B / C-flat, F-sharp / G-flat, C-sharp / D-flat. */
static void test_enharmonic_keys(void) {
    assert(standard_tonality_major_keys_enharmonic(PITCH_LETTER_B, 0, PITCH_LETTER_C, -1));
    assert(standard_tonality_major_keys_enharmonic(PITCH_LETTER_C, -1, PITCH_LETTER_B, 0));
    assert(standard_tonality_major_keys_enharmonic(PITCH_LETTER_F, 1, PITCH_LETTER_G, -1));
    assert(standard_tonality_major_keys_enharmonic(PITCH_LETTER_C, 1, PITCH_LETTER_D, -1));

    /* The signatures differ: 5 sharps vs 7 flats, 6 vs 6, 7 vs 5. */
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_B, 0) == 5);
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_C, -1) == -7);

    /* Not enharmonic: different sound, the same key, or an imaginary key. */
    assert(!standard_tonality_major_keys_enharmonic(PITCH_LETTER_C, 0, PITCH_LETTER_D, 0));
    assert(!standard_tonality_major_keys_enharmonic(PITCH_LETTER_B, 0, PITCH_LETTER_B, 0));
    assert(!standard_tonality_major_keys_enharmonic(PITCH_LETTER_G, 1, PITCH_LETTER_A, -1)); /* G-sharp is imaginary */
    assert(!standard_tonality_major_keys_enharmonic(PITCH_LETTER_E, 0, PITCH_LETTER_F, -1)); /* F-flat is imaginary */
}

/* Property: exactly the three source pairs among all real keys. */
static void test_only_three_enharmonic_pairs_exist(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int pairs = 0;
    for (size_t a = 0; a < 7; a++) {
        for (int acc_a = -1; acc_a <= 1; acc_a++) {
            for (size_t b = 0; b < 7; b++) {
                for (int acc_b = -1; acc_b <= 1; acc_b++) {
                    if (standard_tonality_major_keys_enharmonic(letters[a], acc_a, letters[b], acc_b)) {
                        pairs++;
                    }
                }
            }
        }
    }
    assert(pairs == 6); /* three pairs, counted in both orders */
}

/* REAL_IMAGINARY: fifteen real keys; F-flat major needs B double-flat. */
static void test_real_and_imaginary_keys(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int real = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            if (standard_tonality_major_key_is_real(letters[l], accidental)) {
                real++;
            }
        }
    }
    assert(real == 15);

    assert(standard_tonality_major_key_is_real(PITCH_LETTER_C, -1));
    assert(standard_tonality_major_key_is_real(PITCH_LETTER_C, 1));
    assert(!standard_tonality_major_key_is_real(PITCH_LETTER_F, -1)); /* F-flat: 8 flats */
    assert(standard_tonality_major_key_accidentals(PITCH_LETTER_F, -1) == -8);
    assert(!standard_tonality_major_key_is_real(PITCH_LETTER_G, 1));  /* G-sharp: 8 sharps */
    assert(!standard_tonality_major_key_is_real(PITCH_LETTER_B, 1));
}

/* Property: the signature built for a real key belongs back to that key,
   and the built signature follows the conventional order. */
static void test_signature_round_trips_to_key(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int round_trips = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            KeySignature *key_signature = NULL;
            if (!standard_tonality_major_key_is_real(letters[l], accidental)) {
                assert(!standard_tonality_major_key_signature(letters[l], accidental, &key_signature));
                assert(key_signature == NULL);
                continue;
            }
            assert(standard_tonality_major_key_signature(letters[l], accidental, &key_signature));
            assert(standard_tonality_is_conventional_signature(key_signature));

            PitchLetter letter;
            int back_accidental;
            assert(standard_tonality_major_key_of_signature(key_signature, &letter, &back_accidental));
            assert(letter == letters[l] && back_accidental == accidental);
            round_trips++;
            key_signature_destroy(key_signature);
        }
    }
    assert(round_trips == 15);
}

static void test_signature_contents(void) {
    KeySignature *key_signature = NULL;
    assert(standard_tonality_major_key_signature(PITCH_LETTER_E, -1, &key_signature)); /* E-flat: B E A flat */
    assert(key_signature_entry_count(key_signature) == 3);
    assert(key_signature_letter_at(key_signature, 0) == PITCH_LETTER_B);
    assert(key_signature_letter_at(key_signature, 1) == PITCH_LETTER_E);
    assert(key_signature_letter_at(key_signature, 2) == PITCH_LETTER_A);
    assert(key_signature_accidental_at(key_signature, 2) == -1);
    key_signature_destroy(key_signature);

    assert(standard_tonality_major_key_signature(PITCH_LETTER_C, 0, &key_signature)); /* C: nothing */
    assert(key_signature_entry_count(key_signature) == 0);
    key_signature_destroy(key_signature);

    assert(standard_tonality_major_key_signature(PITCH_LETTER_D, 0, &key_signature)); /* D: F C sharp */
    assert(key_signature_entry_count(key_signature) == 2);
    assert(key_signature_letter_at(key_signature, 0) == PITCH_LETTER_F);
    assert(key_signature_accidental_at(key_signature, 1) == 1);
    key_signature_destroy(key_signature);
}

/* MINOR_RING: each minor key sits beside the major key with its
   signature -- same count, a minor third (three fifths) lower. */
static void test_minor_ring_sits_beside_major_keys(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            if (!standard_tonality_major_key_is_real(letters[l], accidental)) {
                continue;
            }
            PitchLetter minor_letter;
            int minor_accidental;
            assert(standard_tonality_relative_minor(letters[l], accidental, &minor_letter, &minor_accidental));
            assert(standard_tonality_minor_key_accidentals(minor_letter, minor_accidental) ==
                   standard_tonality_major_key_accidentals(letters[l], accidental));
            assert(standard_tonality_minor_key_is_real(minor_letter, minor_accidental));
        }
    }
    /* C major beside A minor at the top of the circle. */
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_A, 0) ==
           standard_tonality_major_key_accidentals(PITCH_LETTER_C, 0));
}

/* The fifth-neighbor step walks the minor ring too: A E B F-sharp ... one
   more sharp each step clockwise. */
static void test_minor_neighbors(void) {
    PitchLetter letters[] = {PITCH_LETTER_E, PITCH_LETTER_B, PITCH_LETTER_F, PITCH_LETTER_C,
                             PITCH_LETTER_G, PITCH_LETTER_D, PITCH_LETTER_A};
    int accidentals[] = {0, 0, 1, 1, 1, 1, 1};

    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 0;
    for (int step = 1; step <= 7; step++) {
        PitchLetter next_letter;
        int next_accidental;
        assert(standard_tonality_circle_neighbor(letter, accidental, true, &next_letter, &next_accidental));
        assert(next_letter == letters[step - 1] && next_accidental == accidentals[step - 1]);
        assert(standard_tonality_minor_key_accidentals(next_letter, next_accidental) == step);
        letter = next_letter;
        accidental = next_accidental;
    }

    /* Counter-clockwise from A minor: D minor, one flat. */
    PitchLetter flat_letter;
    int flat_accidental;
    assert(standard_tonality_circle_neighbor(PITCH_LETTER_A, 0, false, &flat_letter, &flat_accidental));
    assert(flat_letter == PITCH_LETTER_D && flat_accidental == 0);
    assert(standard_tonality_minor_key_accidentals(flat_letter, flat_accidental) == -1);
}

/* The bottom of the circle pairs G-sharp / A-flat minor, D-sharp / E-flat
   minor, A-sharp / B-flat minor. */
static void test_minor_enharmonic_keys(void) {
    assert(standard_tonality_minor_keys_enharmonic(PITCH_LETTER_G, 1, PITCH_LETTER_A, -1));
    assert(standard_tonality_minor_keys_enharmonic(PITCH_LETTER_A, -1, PITCH_LETTER_G, 1));
    assert(standard_tonality_minor_keys_enharmonic(PITCH_LETTER_D, 1, PITCH_LETTER_E, -1));
    assert(standard_tonality_minor_keys_enharmonic(PITCH_LETTER_A, 1, PITCH_LETTER_B, -1));

    /* Different sound, the same key, or an imaginary key. */
    assert(!standard_tonality_minor_keys_enharmonic(PITCH_LETTER_A, 0, PITCH_LETTER_B, 0));
    assert(!standard_tonality_minor_keys_enharmonic(PITCH_LETTER_G, 1, PITCH_LETTER_G, 1));
    assert(!standard_tonality_minor_keys_enharmonic(PITCH_LETTER_E, 1, PITCH_LETTER_F, 0)); /* E-sharp minor */
    assert(!standard_tonality_minor_keys_enharmonic(PITCH_LETTER_D, -1, PITCH_LETTER_C, 1)); /* D-flat minor */

    /* The paired signatures differ, as in major: 5 sharps vs 7 flats. */
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_G, 1) == 5);
    assert(standard_tonality_minor_key_accidentals(PITCH_LETTER_A, -1) == -7);
}

/* Property: exactly the three minor pairs among real minor keys. */
static void test_only_three_minor_enharmonic_pairs_exist(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int pairs = 0;
    for (size_t a = 0; a < 7; a++) {
        for (int acc_a = -1; acc_a <= 1; acc_a++) {
            for (size_t b = 0; b < 7; b++) {
                for (int acc_b = -1; acc_b <= 1; acc_b++) {
                    if (standard_tonality_minor_keys_enharmonic(letters[a], acc_a, letters[b], acc_b)) {
                        pairs++;
                    }
                }
            }
        }
    }
    assert(pairs == 6); /* three pairs, counted in both orders */
}

int main(void) {
    test_rules_reference_framework();
    test_accidental_counts();
    test_neighbors_clockwise();
    test_neighbors_counter_clockwise();
    test_neighbors_are_inverse();
    test_neighbor_rejects_beyond_double_accidentals();
    test_enharmonic_keys();
    test_only_three_enharmonic_pairs_exist();
    test_real_and_imaginary_keys();
    test_signature_round_trips_to_key();
    test_signature_contents();
    test_minor_ring_sits_beside_major_keys();
    test_minor_neighbors();
    test_minor_enharmonic_keys();
    test_only_three_minor_enharmonic_pairs_exist();
    printf("test_circle_of_fifths_rules (standard-tonality): ok\n");
    return 0;
}
