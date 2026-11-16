#include "pitch/key_signature.h"

#include <assert.h>
#include <stddef.h>

static void test_empty_signature(void) {
    KeySignature *key_signature = NULL;
    assert(key_signature_create(NULL, 0, &key_signature) == KEY_SIGNATURE_OK);
    assert(key_signature != NULL);
    assert(key_signature_entry_count(key_signature) == 0);
    key_signature_destroy(key_signature);
}

static void test_create_rejects_null_entries_with_count(void) {
    KeySignature *key_signature = NULL;
    assert(key_signature_create(NULL, 3, &key_signature) == KEY_SIGNATURE_ERR_NULL_ENTRIES);
    assert(key_signature == NULL);
}

static void test_create_rejects_invalid_letter(void) {
    KeySignatureEntry entries[] = {{(PitchLetter)99, 1}};
    KeySignature *key_signature = NULL;
    assert(key_signature_create(entries, 1, &key_signature) == KEY_SIGNATURE_ERR_INVALID_LETTER);
    assert(key_signature == NULL);
}

static void test_create_rejects_out_of_range_accidental(void) {
    KeySignatureEntry too_sharp[] = {{PITCH_LETTER_F, 3}};
    KeySignatureEntry too_flat[] = {{PITCH_LETTER_B, -3}};
    KeySignature *key_signature = NULL;
    assert(key_signature_create(too_sharp, 1, &key_signature) == KEY_SIGNATURE_ERR_INVALID_ACCIDENTAL);
    assert(key_signature_create(too_flat, 1, &key_signature) == KEY_SIGNATURE_ERR_INVALID_ACCIDENTAL);
    assert(key_signature == NULL);
}

static void test_entries_keep_order(void) {
    KeySignatureEntry entries[] = {{PITCH_LETTER_B, -1}, {PITCH_LETTER_E, -1}, {PITCH_LETTER_A, -1}};
    KeySignature *key_signature = NULL;
    assert(key_signature_create(entries, 3, &key_signature) == KEY_SIGNATURE_OK);
    assert(key_signature_entry_count(key_signature) == 3);
    assert(key_signature_letter_at(key_signature, 0) == PITCH_LETTER_B);
    assert(key_signature_letter_at(key_signature, 1) == PITCH_LETTER_E);
    assert(key_signature_letter_at(key_signature, 2) == PITCH_LETTER_A);
    for (size_t i = 0; i < 3; i++) {
        assert(key_signature_accidental_at(key_signature, i) == -1);
    }
    key_signature_destroy(key_signature);
}

/* The signature copies its entries: later changes to the caller's array
   do not reach it. */
static void test_entries_are_copied(void) {
    KeySignatureEntry entries[] = {{PITCH_LETTER_F, 1}};
    KeySignature *key_signature = NULL;
    assert(key_signature_create(entries, 1, &key_signature) == KEY_SIGNATURE_OK);
    entries[0].letter = PITCH_LETTER_C;
    entries[0].accidental = -1;
    assert(key_signature_letter_at(key_signature, 0) == PITCH_LETTER_F);
    assert(key_signature_accidental_at(key_signature, 0) == 1);
    key_signature_destroy(key_signature);
}

/* The type carries no theory: mixed accidentals and a double sharp are
   representable; rules about them live in frameworks. */
static void test_type_carries_no_theory(void) {
    KeySignatureEntry entries[] = {{PITCH_LETTER_F, 1}, {PITCH_LETTER_B, -2}, {PITCH_LETTER_F, 0}};
    KeySignature *key_signature = NULL;
    assert(key_signature_create(entries, 3, &key_signature) == KEY_SIGNATURE_OK);
    assert(key_signature_accidental_at(key_signature, 1) == -2);
    key_signature_destroy(key_signature);
}

static void test_destroy_null_is_safe(void) {
    key_signature_destroy(NULL);
}

int main(void) {
    test_empty_signature();
    test_create_rejects_null_entries_with_count();
    test_create_rejects_invalid_letter();
    test_create_rejects_out_of_range_accidental();
    test_entries_keep_order();
    test_entries_are_copied();
    test_type_carries_no_theory();
    test_destroy_null_is_safe();
    return 0;
}
