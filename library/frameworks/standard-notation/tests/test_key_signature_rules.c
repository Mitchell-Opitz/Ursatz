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

/* The first `count` accidentals of the standard order, all `accidental`. */
static KeySignature *make_standard(int accidental, size_t count) {
    KeySignatureEntry entries[STANDARD_NOTATION_KEY_SIGNATURE_MAX_ACCIDENTALS];
    for (size_t i = 0; i < count; i++) {
        entries[i].letter =
            accidental > 0 ? standard_notation_sharp_order_letter(i) : standard_notation_flat_order_letter(i);
        entries[i].accidental = accidental;
    }
    return make_signature(entries, count);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_KEY_SIGNATURE_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_key_signature_rule((StandardNotationKeySignatureRule)i);
        const Constraint *constraint = standard_notation_key_signature_constraint((StandardNotationKeySignatureRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_key_signature_rule(STANDARD_NOTATION_KEY_SIGNATURE_RULE_COUNT) == NULL);
}

static void test_sharp_order(void) {
    PitchLetter expected[] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                              PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
    for (size_t i = 0; i < 7; i++) {
        assert(standard_notation_sharp_order_letter(i) == expected[i]);
    }
}

static void test_flat_order(void) {
    PitchLetter expected[] = {PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A, PITCH_LETTER_D,
                              PITCH_LETTER_G, PITCH_LETTER_C, PITCH_LETTER_F};
    for (size_t i = 0; i < 7; i++) {
        assert(standard_notation_flat_order_letter(i) == expected[i]);
    }
}

/* Property: the flat order is the sharp order reversed. */
static void test_order_palindrome(void) {
    for (size_t i = 0; i < 7; i++) {
        assert(standard_notation_flat_order_letter(i) == standard_notation_sharp_order_letter(6 - i));
    }
}

static void test_standard_signatures_follow_order(void) {
    for (size_t count = 0; count <= 7; count++) {
        KeySignature *sharps = make_standard(1, count);
        KeySignature *flats = make_standard(-1, count);
        assert(standard_notation_key_signature_follows_order(sharps));
        assert(standard_notation_key_signature_follows_order(flats));
        key_signature_destroy(sharps);
        key_signature_destroy(flats);
    }
}

static void test_sharps_and_flats_classification(void) {
    KeySignature *empty = make_standard(1, 0);
    KeySignature *sharps = make_standard(1, 3);
    KeySignature *flats = make_standard(-1, 3);
    KeySignatureEntry mixed_entries[] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_B, -1}};
    KeySignature *mixed = make_signature(mixed_entries, 2);

    assert(standard_notation_key_signature_is_sharps(sharps));
    assert(!standard_notation_key_signature_is_flats(sharps));
    assert(standard_notation_key_signature_is_flats(flats));
    assert(!standard_notation_key_signature_is_sharps(flats));
    assert(!standard_notation_key_signature_is_sharps(empty));
    assert(!standard_notation_key_signature_is_flats(empty));
    assert(!standard_notation_key_signature_is_sharps(mixed));
    assert(!standard_notation_key_signature_is_flats(mixed));
    assert(!standard_notation_key_signature_is_sharps(NULL));
    assert(!standard_notation_key_signature_is_flats(NULL));

    key_signature_destroy(empty);
    key_signature_destroy(sharps);
    key_signature_destroy(flats);
    key_signature_destroy(mixed);
}

static void test_out_of_order_signatures_are_rejected(void) {
    /* C before F; skipping a letter; flats in sharp order; mixed; doubles. */
    KeySignatureEntry swapped[] = {{PITCH_LETTER_C, 1}, {PITCH_LETTER_F, 1}};
    KeySignatureEntry skipped[] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_G, 1}};
    KeySignatureEntry flats_in_sharp_order[] = {{PITCH_LETTER_F, -1}, {PITCH_LETTER_C, -1}};
    KeySignatureEntry mixed[] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_C, -1}};
    KeySignatureEntry doubled[] = {{PITCH_LETTER_F, 2}};
    KeySignatureEntry natural[] = {{PITCH_LETTER_F, 0}};
    KeySignatureEntry too_many[8] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_C, 1}, {PITCH_LETTER_G, 1},
                                     {PITCH_LETTER_D, 1}, {PITCH_LETTER_A, 1}, {PITCH_LETTER_E, 1},
                                     {PITCH_LETTER_B, 1}, {PITCH_LETTER_F, 1}};

    KeySignature *signatures[] = {make_signature(swapped, 2),  make_signature(skipped, 2),
                                  make_signature(flats_in_sharp_order, 2), make_signature(mixed, 2),
                                  make_signature(doubled, 1),  make_signature(natural, 1),
                                  make_signature(too_many, 8)};
    for (size_t i = 0; i < sizeof(signatures) / sizeof(signatures[0]); i++) {
        assert(!standard_notation_key_signature_follows_order(signatures[i]));
        key_signature_destroy(signatures[i]);
    }
    assert(!standard_notation_key_signature_follows_order(NULL));
}

static void test_signature_applies_in_every_octave(void) {
    KeySignature *flats = make_standard(-1, 3); /* B E A flat */
    assert(standard_notation_key_signature_alteration(flats, PITCH_LETTER_B) == -1);
    assert(standard_notation_key_signature_alteration(flats, PITCH_LETTER_E) == -1);
    assert(standard_notation_key_signature_alteration(flats, PITCH_LETTER_A) == -1);
    /* Letters outside the signature are unaltered. */
    assert(standard_notation_key_signature_alteration(flats, PITCH_LETTER_D) == 0);
    assert(standard_notation_key_signature_alteration(flats, PITCH_LETTER_C) == 0);
    /* The lookup takes a letter only, so it is the same for every octave. */
    assert(standard_notation_key_signature_alteration(NULL, PITCH_LETTER_B) == 0);
    key_signature_destroy(flats);

    KeySignature *sharps = make_standard(1, 2); /* F C sharp */
    assert(standard_notation_key_signature_alteration(sharps, PITCH_LETTER_F) == 1);
    assert(standard_notation_key_signature_alteration(sharps, PITCH_LETTER_C) == 1);
    assert(standard_notation_key_signature_alteration(sharps, PITCH_LETTER_G) == 0);
    key_signature_destroy(sharps);
}

static void test_placement(void) {
    /* clef, key, time: alphabetical. */
    assert(standard_notation_key_signature_placement_valid(0, 1, 2));
    assert(standard_notation_key_signature_placement_valid(0, 5, 9));
    assert(!standard_notation_key_signature_placement_valid(1, 0, 2)); /* key before clef */
    assert(!standard_notation_key_signature_placement_valid(0, 2, 1)); /* key after time */
    assert(!standard_notation_key_signature_placement_valid(0, 0, 2)); /* key on the clef */
    assert(!standard_notation_key_signature_placement_valid(0, 2, 2)); /* key on the time signature */
}

/* MINOR_SAME_ORDER: minor signatures are their relative majors', so the
   same order holds -- A minor has none, E minor F-sharp, D minor B-flat. */
static void test_minor_signatures_follow_the_same_order(void) {
    for (size_t count = 0; count <= 7; count++) {
        KeySignature *sharps = make_standard(1, count);
        KeySignature *flats = make_standard(-1, count);
        assert(standard_notation_minor_key_signature_follows_order(sharps));
        assert(standard_notation_minor_key_signature_follows_order(flats));
        assert(standard_notation_minor_key_signature_follows_order(sharps) ==
               standard_notation_key_signature_follows_order(sharps));
        key_signature_destroy(sharps);
        key_signature_destroy(flats);
    }

    KeySignatureEntry swapped[] = {{PITCH_LETTER_C, 1}, {PITCH_LETTER_F, 1}};
    KeySignature *out_of_order = make_signature(swapped, 2);
    assert(!standard_notation_minor_key_signature_follows_order(out_of_order));
    assert(!standard_notation_minor_key_signature_follows_order(NULL));
    key_signature_destroy(out_of_order);
}

int main(void) {
    test_rules_reference_framework();
    test_sharp_order();
    test_flat_order();
    test_order_palindrome();
    test_standard_signatures_follow_order();
    test_sharps_and_flats_classification();
    test_out_of_order_signatures_are_rejected();
    test_signature_applies_in_every_octave();
    test_placement();
    test_minor_signatures_follow_the_same_order();
    printf("test_key_signature_rules (standard-notation): ok\n");
    return 0;
}
