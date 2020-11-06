#ifndef MUSIC_PITCH_KEY_SIGNATURE_H
#define MUSIC_PITCH_KEY_SIGNATURE_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"

/*
 * KeySignature (Value): an ordered collection of accidentals, each
 * attached to a letter, written at the start of a composition so every
 * note of that letter carries the accidental in every octave. Order is
 * meaningful -- it is the order the accidentals are written in -- so two
 * KeySignatures with the same entries in a different order are not equal.
 *
 * Each entry is a letter and a semitone offset (-2..+2), the same
 * integer PitchSpelling already uses for its accidental field
 * (pitch/pitch_spelling.h, pitch/accidental.h). An empty KeySignature
 * (count 0) is valid: it is the signature with no sharps or flats.
 *
 * This type carries no theory. Which entries make a conventional
 * signature (sharps in the order F C G D A E B, flats in the reverse
 * order, at most seven) and which key a signature belongs to are rules
 * of a Framework (frameworks/standard-notation, frameworks/
 * standard-tonality), not facts of this type -- so a signature with
 * mixed or repeated accidentals is representable here.
 *
 * A KeySignature is a Value, not an Entity -- it carries no EntityID of
 * its own (same as Scale, semantics/scale.h). It has no mutator: a
 * revision is a new KeySignature construction, not an in-place field
 * change (Law C2).
 */

typedef struct KeySignature KeySignature;

typedef struct {
    PitchLetter letter;
    int accidental;
} KeySignatureEntry;

typedef enum {
    KEY_SIGNATURE_OK = 0,
    KEY_SIGNATURE_ERR_NULL_ENTRIES,
    KEY_SIGNATURE_ERR_INVALID_LETTER,
    KEY_SIGNATURE_ERR_INVALID_ACCIDENTAL,
    KEY_SIGNATURE_ERR_ALLOC
} KeySignatureResult;

/*
 * entries is copied. entries may be NULL only when entry_count is 0 (the
 * empty signature). Every letter must be a valid PitchLetter and every
 * accidental within ACCIDENTAL_MIN_OFFSET..ACCIDENTAL_MAX_OFFSET.
 */
KeySignatureResult key_signature_create(const KeySignatureEntry *entries, size_t entry_count, KeySignature **out);
void key_signature_destroy(KeySignature *key_signature);

size_t key_signature_entry_count(const KeySignature *key_signature);

/* index must be < key_signature_entry_count. */
PitchLetter key_signature_letter_at(const KeySignature *key_signature, size_t index);
int key_signature_accidental_at(const KeySignature *key_signature, size_t index);

/* Value equality: same entries in the same order. */
bool key_signature_equals(const KeySignature *a, const KeySignature *b);

#endif
