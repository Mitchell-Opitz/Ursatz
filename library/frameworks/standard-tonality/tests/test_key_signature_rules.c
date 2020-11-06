#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "key_signature_rules.h"
#include "spelling_test_util.h"

static KeySignature *make_signature(const KeySignatureEntry *entries, size_t count) {
    KeySignature *key_signature = NULL;
    assert(key_signature_create(entries, count, &key_signature) == KEY_SIGNATURE_OK);
    return key_signature;
}

static const PitchLetter SHARPS[7] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                                      PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
static const PitchLetter FLATS[7] = {PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A, PITCH_LETTER_D,
                                     PITCH_LETTER_G, PITCH_LETTER_C, PITCH_LETTER_F};

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

static void check_major_key(const KeySignature *key_signature, PitchLetter expected_letter, int expected_accidental) {
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 99;
    assert(standard_tonality_major_key_of_signature(key_signature, &letter, &accidental));
    assert(letter == expected_letter);
    assert(accidental == expected_accidental);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_key_signature_rule((StandardTonalityKeySignatureRule)i);
        const Constraint *constraint = standard_tonality_key_signature_constraint((StandardTonalityKeySignatureRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_key_signature_rule(STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT) == NULL);
}

/* SHARP_TONIC: the last sharp is a half step below the tonic. Source
   examples: one sharp is G major, three is A major, six is F-sharp major. */
static void test_sharp_signatures(void) {
    PitchLetter letters[] = {PITCH_LETTER_G, PITCH_LETTER_D, PITCH_LETTER_A, PITCH_LETTER_E,
                             PITCH_LETTER_B, PITCH_LETTER_F, PITCH_LETTER_C};
    int accidentals[] = {0, 0, 0, 0, 0, 1, 1}; /* G D A E B F-sharp C-sharp */
    for (size_t count = 1; count <= 7; count++) {
        KeySignature *key_signature = make_sharps(count);
        check_major_key(key_signature, letters[count - 1], accidentals[count - 1]);
        key_signature_destroy(key_signature);
    }
}

/* FLAT_TONIC: the second-to-last flat is the tonic. Source examples: two
   flats is B-flat major, four is A-flat major, six is G-flat major. */
static void test_flat_signatures(void) {
    /* One flat is F major (MEMORIZED); two to seven follow the rule. */
    PitchLetter letters[] = {PITCH_LETTER_F, PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A,
                             PITCH_LETTER_D, PITCH_LETTER_G, PITCH_LETTER_C};
    int accidentals[] = {0, -1, -1, -1, -1, -1, -1};
    for (size_t count = 1; count <= 7; count++) {
        KeySignature *key_signature = make_flats(count);
        check_major_key(key_signature, letters[count - 1], accidentals[count - 1]);
        key_signature_destroy(key_signature);
    }
}

/* MEMORIZED: C major has nothing, F major has one flat. */
static void test_memorized_signatures(void) {
    KeySignature *none = make_sharps(0);
    KeySignature *one_flat = make_flats(1);
    check_major_key(none, PITCH_LETTER_C, 0);
    check_major_key(one_flat, PITCH_LETTER_F, 0);
    key_signature_destroy(none);
    key_signature_destroy(one_flat);
}

/* C_FAMILY: nothing is C major, every note flat is C-flat major, every
   note sharp is C-sharp major. */
static void test_c_family(void) {
    KeySignature *none = make_sharps(0);
    KeySignature *all_sharp = make_sharps(7);
    KeySignature *all_flat = make_flats(7);
    check_major_key(none, PITCH_LETTER_C, 0);
    check_major_key(all_sharp, PITCH_LETTER_C, 1);
    check_major_key(all_flat, PITCH_LETTER_C, -1);
    key_signature_destroy(none);
    key_signature_destroy(all_sharp);
    key_signature_destroy(all_flat);
}

static void test_conventional_signatures(void) {
    for (size_t count = 0; count <= 7; count++) {
        KeySignature *sharps = make_sharps(count);
        KeySignature *flats = make_flats(count);
        assert(standard_tonality_is_conventional_signature(sharps));
        assert(standard_tonality_is_conventional_signature(flats));
        key_signature_destroy(sharps);
        key_signature_destroy(flats);
    }
    assert(!standard_tonality_is_conventional_signature(NULL));
}

static void test_unconventional_signatures_have_no_key(void) {
    KeySignatureEntry swapped[] = {{PITCH_LETTER_C, 1}, {PITCH_LETTER_F, 1}};
    KeySignatureEntry mixed[] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_B, -1}};
    KeySignatureEntry doubled[] = {{PITCH_LETTER_F, 2}};
    KeySignatureEntry natural[] = {{PITCH_LETTER_F, 0}};
    KeySignatureEntry flats_in_sharp_order[] = {{PITCH_LETTER_F, -1}, {PITCH_LETTER_C, -1}};
    KeySignatureEntry too_many[8] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_C, 1}, {PITCH_LETTER_G, 1},
                                     {PITCH_LETTER_D, 1}, {PITCH_LETTER_A, 1}, {PITCH_LETTER_E, 1},
                                     {PITCH_LETTER_B, 1}, {PITCH_LETTER_F, 1}};
    KeySignature *signatures[] = {make_signature(swapped, 2), make_signature(mixed, 2),
                                  make_signature(doubled, 1), make_signature(natural, 1),
                                  make_signature(flats_in_sharp_order, 2), make_signature(too_many, 8)};

    for (size_t i = 0; i < sizeof(signatures) / sizeof(signatures[0]); i++) {
        PitchLetter letter = PITCH_LETTER_D;
        int accidental = 42;
        assert(!standard_tonality_is_conventional_signature(signatures[i]));
        assert(!standard_tonality_major_key_of_signature(signatures[i], &letter, &accidental));
        assert(letter == PITCH_LETTER_D && accidental == 42); /* untouched */
        key_signature_destroy(signatures[i]);
    }

    PitchLetter letter;
    int accidental;
    KeySignature *none = make_sharps(0);
    assert(!standard_tonality_major_key_of_signature(NULL, &letter, &accidental));
    assert(!standard_tonality_major_key_of_signature(none, NULL, &accidental));
    assert(!standard_tonality_major_key_of_signature(none, &letter, NULL));
    key_signature_destroy(none);
}

int main(void) {
    test_rules_reference_framework();
    test_sharp_signatures();
    test_flat_signatures();
    test_memorized_signatures();
    test_c_family();
    test_conventional_signatures();
    test_unconventional_signatures_have_no_key();
    printf("test_key_signature_rules (standard-tonality): ok\n");
    return 0;
}
