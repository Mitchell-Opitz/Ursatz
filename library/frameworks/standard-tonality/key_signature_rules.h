#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_KEY_SIGNATURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_KEY_SIGNATURE_RULES_H

#include <stdbool.h>

#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Key signature to major key rules for standard-tonality-v1. A key
 * signature collects a major scale's accidentals; these rules say which
 * major key a signature belongs to:
 *
 *   SHARP_TONIC   in a sharp signature the last sharp is a half step
 *                 below the tonic
 *   FLAT_TONIC    in a flat signature the second-to-last flat is the
 *                 tonic
 *   MEMORIZED     two signatures have no such trick: C major has no
 *                 sharps or flats, F major has one flat (B-flat)
 *   C_FAMILY      C major is the signature with nothing, C-flat major
 *                 the one with every note flat (7 flats), and C-sharp
 *                 major the one with every note sharp (7 sharps)
 *
 * A "conventional" signature is none, or n sharps (n <= 7) written in
 * the order F C G D A E B, or n flats written in the order B E A D G C
 * F. How signatures are written on a staff is notation, and lives in
 * standard-notation; this module keeps its own copy of the letter
 * order, since framework modules do not depend on each other.
 */

typedef enum {
    STANDARD_TONALITY_KEY_SIGNATURE_RULE_SHARP_TONIC = 0,
    STANDARD_TONALITY_KEY_SIGNATURE_RULE_FLAT_TONIC,
    STANDARD_TONALITY_KEY_SIGNATURE_RULE_MEMORIZED,
    STANDARD_TONALITY_KEY_SIGNATURE_RULE_C_FAMILY,
    STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT
} StandardTonalityKeySignatureRule;

const Rule *standard_tonality_key_signature_rule(StandardTonalityKeySignatureRule which);
const Constraint *standard_tonality_key_signature_constraint(StandardTonalityKeySignatureRule which);

/* True for the empty signature and for conventional sharp or flat
   signatures (see above). False for NULL, mixed or non-single
   accidentals, out-of-order letters, and more than 7 entries. */
bool standard_tonality_is_conventional_signature(const KeySignature *key_signature);

/*
 * The major key a conventional signature belongs to, by SHARP_TONIC,
 * FLAT_TONIC, MEMORIZED and C_FAMILY. On true, *letter and *accidental
 * give the tonic's name (e.g. G and 0 for one sharp, B and -1 for two
 * flats, C and +1 for seven sharps). False (outputs untouched) for a
 * non-conventional signature or NULL.
 */
bool standard_tonality_major_key_of_signature(const KeySignature *key_signature, PitchLetter *letter,
                                              int *accidental);

#endif
