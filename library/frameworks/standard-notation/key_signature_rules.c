#include "key_signature_rules.h"

#include <stddef.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_KEY_SIGNATURE_RULE_COUNT] = {
    "key-signature-sharp-order", "key-signature-flat-order", "key-signature-order-palindrome",
    "key-signature-all-octaves", "key-signature-placement", "key-signature-minor-same-order"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_KEY_SIGNATURE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_key_signature, StandardNotationKeySignatureRule, STANDARD_NOTATION_KEY_SIGNATURE_RULE_COUNT, g_table)

static const PitchLetter SHARP_ORDER[STANDARD_NOTATION_KEY_SIGNATURE_MAX_ACCIDENTALS] = {
    PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D, PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};

PitchLetter standard_notation_sharp_order_letter(size_t index) {
    return SHARP_ORDER[index];
}

PitchLetter standard_notation_flat_order_letter(size_t index) {
    return SHARP_ORDER[STANDARD_NOTATION_KEY_SIGNATURE_MAX_ACCIDENTALS - 1 - index];
}

static bool all_entries_have_accidental(const KeySignature *key_signature, int accidental) {
    size_t count = key_signature_entry_count(key_signature);
    if (count == 0) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (key_signature_accidental_at(key_signature, i) != accidental) {
            return false;
        }
    }
    return true;
}

bool standard_notation_key_signature_is_sharps(const KeySignature *key_signature) {
    if (key_signature == NULL) {
        return false;
    }
    return all_entries_have_accidental(key_signature, 1);
}

bool standard_notation_key_signature_is_flats(const KeySignature *key_signature) {
    if (key_signature == NULL) {
        return false;
    }
    return all_entries_have_accidental(key_signature, -1);
}

bool standard_notation_key_signature_follows_order(const KeySignature *key_signature) {
    if (key_signature == NULL) {
        return false;
    }
    size_t count = key_signature_entry_count(key_signature);
    if (count == 0) {
        return true;
    }
    if (count > STANDARD_NOTATION_KEY_SIGNATURE_MAX_ACCIDENTALS) {
        return false;
    }

    bool sharps = standard_notation_key_signature_is_sharps(key_signature);
    bool flats = standard_notation_key_signature_is_flats(key_signature);
    if (!sharps && !flats) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        PitchLetter expected = sharps ? standard_notation_sharp_order_letter(i) : standard_notation_flat_order_letter(i);
        if (key_signature_letter_at(key_signature, i) != expected) {
            return false;
        }
    }
    return true;
}

bool standard_notation_minor_key_signature_follows_order(const KeySignature *key_signature) {
    return standard_notation_key_signature_follows_order(key_signature);
}

int standard_notation_key_signature_alteration(const KeySignature *key_signature, PitchLetter letter) {
    if (key_signature == NULL) {
        return 0;
    }
    size_t count = key_signature_entry_count(key_signature);
    for (size_t i = 0; i < count; i++) {
        if (key_signature_letter_at(key_signature, i) == letter) {
            return key_signature_accidental_at(key_signature, i);
        }
    }
    return 0;
}

bool standard_notation_key_signature_placement_valid(size_t clef_position, size_t key_position,
                                                     size_t time_position) {
    return clef_position < key_position && key_position < time_position;
}
