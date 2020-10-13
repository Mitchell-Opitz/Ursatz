#include "pitch/key_signature.h"

#include <stdlib.h>

#include "pitch/accidental.h"

struct KeySignature {
    KeySignatureEntry *entries;
    size_t entry_count;
};

KeySignatureResult key_signature_create(const KeySignatureEntry *entries, size_t entry_count, KeySignature **out) {
    if (entries == NULL && entry_count > 0) {
        return KEY_SIGNATURE_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].letter < PITCH_LETTER_A || entries[i].letter > PITCH_LETTER_G) {
            return KEY_SIGNATURE_ERR_INVALID_LETTER;
        }
        if (entries[i].accidental < ACCIDENTAL_MIN_OFFSET || entries[i].accidental > ACCIDENTAL_MAX_OFFSET) {
            return KEY_SIGNATURE_ERR_INVALID_ACCIDENTAL;
        }
    }

    KeySignature *key_signature = (KeySignature *)malloc(sizeof(KeySignature));
    if (key_signature == NULL) {
        return KEY_SIGNATURE_ERR_ALLOC;
    }

    KeySignatureEntry *owned_entries = NULL;
    if (entry_count > 0) {
        owned_entries = (KeySignatureEntry *)malloc(sizeof(KeySignatureEntry) * entry_count);
        if (owned_entries == NULL) {
            free(key_signature);
            return KEY_SIGNATURE_ERR_ALLOC;
        }
        for (size_t i = 0; i < entry_count; i++) {
            owned_entries[i] = entries[i];
        }
    }

    key_signature->entries = owned_entries;
    key_signature->entry_count = entry_count;

    *out = key_signature;
    return KEY_SIGNATURE_OK;
}

void key_signature_destroy(KeySignature *key_signature) {
    if (key_signature == NULL) {
        return;
    }
    free(key_signature->entries);
    free(key_signature);
}

size_t key_signature_entry_count(const KeySignature *key_signature) {
    return key_signature->entry_count;
}

PitchLetter key_signature_letter_at(const KeySignature *key_signature, size_t index) {
    return key_signature->entries[index].letter;
}

int key_signature_accidental_at(const KeySignature *key_signature, size_t index) {
    return key_signature->entries[index].accidental;
}

bool key_signature_equals(const KeySignature *a, const KeySignature *b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    if (a->entry_count != b->entry_count) {
        return false;
    }
    for (size_t i = 0; i < a->entry_count; i++) {
        if (a->entries[i].letter != b->entries[i].letter || a->entries[i].accidental != b->entries[i].accidental) {
            return false;
        }
    }
    return true;
}
