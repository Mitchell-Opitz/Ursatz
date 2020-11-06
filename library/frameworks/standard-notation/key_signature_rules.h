#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_KEY_SIGNATURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_KEY_SIGNATURE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Key signature rules for standard-notation-v1. A key signature, made of
 * sharps or flats, collects a scale's accidentals at the start of a
 * composition:
 *
 *   SHARP_ORDER   sharps are added in the order F C G D A E B
 *   FLAT_ORDER    flats are added in the order B E A D G C F
 *   ORDER_PALINDROME
 *                 the order of flats is the order of sharps reversed
 *   ALL_OCTAVES   an accidental in the signature applies to every note
 *                 of its letter, in every octave
 *   PLACEMENT     a key signature follows the clef and precedes the time
 *                 signature (clef, key, time -- alphabetical)
 *
 * Which key a signature belongs to is a tonal question, answered by
 * standard-tonality, not here. The staff positions of individual
 * accidentals (the zig-zag layout, which differs by clef) are not
 * modeled; only their order is.
 */

#define STANDARD_NOTATION_KEY_SIGNATURE_MAX_ACCIDENTALS 7

typedef enum {
    STANDARD_NOTATION_KEY_SIGNATURE_RULE_SHARP_ORDER = 0,
    STANDARD_NOTATION_KEY_SIGNATURE_RULE_FLAT_ORDER,
    STANDARD_NOTATION_KEY_SIGNATURE_RULE_ORDER_PALINDROME,
    STANDARD_NOTATION_KEY_SIGNATURE_RULE_ALL_OCTAVES,
    STANDARD_NOTATION_KEY_SIGNATURE_RULE_PLACEMENT,
    STANDARD_NOTATION_KEY_SIGNATURE_RULE_COUNT
} StandardNotationKeySignatureRule;

const Rule *standard_notation_key_signature_rule(StandardNotationKeySignatureRule which);
const Constraint *standard_notation_key_signature_constraint(StandardNotationKeySignatureRule which);

/* SHARP_ORDER: the letter of the index-th sharp (0 = F ... 6 = B).
   index must be < STANDARD_NOTATION_KEY_SIGNATURE_MAX_ACCIDENTALS. */
PitchLetter standard_notation_sharp_order_letter(size_t index);

/* FLAT_ORDER / ORDER_PALINDROME: the letter of the index-th flat
   (0 = B ... 6 = F). Same precondition on index. */
PitchLetter standard_notation_flat_order_letter(size_t index);

/* True when the signature has at least one entry and every entry is a
   sharp / a flat. False for NULL and for the empty signature. */
bool standard_notation_key_signature_is_sharps(const KeySignature *key_signature);
bool standard_notation_key_signature_is_flats(const KeySignature *key_signature);

/* SHARP_ORDER / FLAT_ORDER: true when the signature is empty, or is n
   sharps (n <= 7) that are the first n letters of the sharp order, or n
   flats that are the first n letters of the flat order. Mixed sharps and
   flats, other accidentals, and out-of-order letters are false. False
   for NULL. */
bool standard_notation_key_signature_follows_order(const KeySignature *key_signature);

/* ALL_OCTAVES: the semitone offset the signature applies to every note
   of `letter` in any octave; 0 when the letter is not in the signature.
   False-equivalent (0) for NULL. */
int standard_notation_key_signature_alteration(const KeySignature *key_signature, PitchLetter letter);

/* PLACEMENT: true when clef_position < key_position < time_position
   (positions are left-to-right order of the three items at the start of
   a staff). */
bool standard_notation_key_signature_placement_valid(size_t clef_position, size_t key_position,
                                                     size_t time_position);

#endif
