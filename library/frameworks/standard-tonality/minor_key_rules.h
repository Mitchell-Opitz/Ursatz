#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_MINOR_KEY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_MINOR_KEY_RULES_H

#include <stdbool.h>

#include "minor_scale_rules.h"
#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Minor key rules for standard-tonality-v1. A minor key is named by its
 * tonic (a letter and a semitone accidental offset), and:
 *
 *   NATURAL_BASIS   minor keys and minor key signatures are just "minor"
 *                   and rest on the natural minor scale, whatever forms
 *                   of the scale a piece uses
 *   SIGNATURE_TONIC a minor key's signature is its relative major's, so
 *                   the minor key of a signature is the relative minor
 *                   of the signature's major key (A minor has no sharps
 *                   or flats; the sharp minor keys run A E B F-sharp
 *                   C-sharp G-sharp D-sharp A-sharp, the flat ones A D
 *                   G C F B-flat E-flat A-flat)
 *   REAL_IMAGINARY  a minor key is real when its signature needs at most
 *                   7 sharps or flats; one that would need a double
 *                   accidental is imaginary
 *   CANDIDATES      a key signature narrows a piece to two keys: a major
 *                   key and its relative minor. Which of the two is a
 *                   judgement from context (the first and last notes,
 *                   say) and is not decided here
 *
 * Signatures are conventional (standard_tonality_is_conventional_
 * signature, key_signature_rules.h); the order and staff placement of
 * minor signatures is the same as major and lives in standard-notation.
 */

typedef enum {
    STANDARD_TONALITY_MINOR_KEY_RULE_NATURAL_BASIS = 0,
    STANDARD_TONALITY_MINOR_KEY_RULE_SIGNATURE_TONIC,
    STANDARD_TONALITY_MINOR_KEY_RULE_REAL_IMAGINARY,
    STANDARD_TONALITY_MINOR_KEY_RULE_CANDIDATES,
    STANDARD_TONALITY_MINOR_KEY_RULE_COUNT
} StandardTonalityMinorKeyRule;

const Rule *standard_tonality_minor_key_rule(StandardTonalityMinorKeyRule which);
const Constraint *standard_tonality_minor_key_constraint(StandardTonalityMinorKeyRule which);

/* NATURAL_BASIS: the scale form a minor key and its signature rest on. */
StandardTonalityMinorForm standard_tonality_minor_key_scale_form(void);

/* SIGNATURE_TONIC: the minor key a conventional signature belongs to. On
   true *letter and *accidental name its tonic (A and 0 for none, B and
   0 for two sharps, G and 0 for two flats, A and +1 for seven sharps).
   False (outputs untouched) for a non-conventional signature or NULL. */
bool standard_tonality_minor_key_of_signature(const KeySignature *key_signature, PitchLetter *letter,
                                              int *accidental);

/* REAL_IMAGINARY: signed accidental count of the minor key with this
   tonic: +n for n sharps, -n for n flats (the same count as its relative
   major's). Beyond +-7 the key is imaginary. */
int standard_tonality_minor_key_accidentals(PitchLetter letter, int accidental);

/* REAL_IMAGINARY: true when the key has at most 7 sharps or flats. */
bool standard_tonality_minor_key_is_real(PitchLetter letter, int accidental);

/* The key signature of a real minor key. False, leaving *out untouched,
   for an imaginary key or on allocation failure. The caller destroys
   *out with key_signature_destroy. */
bool standard_tonality_minor_key_signature(PitchLetter letter, int accidental, KeySignature **out);

/* CANDIDATES: the major key and the relative minor key a conventional
   signature allows. False (outputs untouched) for a non-conventional
   signature, NULL, or NULL outputs. */
bool standard_tonality_signature_key_candidates(const KeySignature *key_signature, PitchLetter *major_letter,
                                                int *major_accidental, PitchLetter *minor_letter,
                                                int *minor_accidental);

#endif
