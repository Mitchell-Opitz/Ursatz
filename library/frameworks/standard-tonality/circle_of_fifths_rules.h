#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_CIRCLE_OF_FIFTHS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_CIRCLE_OF_FIFTHS_RULES_H

#include <stdbool.h>

#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Circle of fifths rules for standard-tonality-v1. The circle places
 * the major keys in order of number of accidentals, C major (none) at
 * the top:
 *
 *   ORDER         clockwise from C each key adds one sharp, counter-
 *                 clockwise each adds one flat
 *   NEIGHBORS     each key is a fifth away from the keys on either side
 *   ENHARMONIC    at the bottom of the circle three pairs of keys sound
 *                 the same under different signatures: B and C-flat
 *                 (5 sharps / 7 flats), F-sharp and G-flat (6 / 6),
 *                 C-sharp and D-flat (7 / 5)
 *   REAL_IMAGINARY
 *                 a major key is "real" when its signature needs at
 *                 most 7 sharps or flats; if it would need a double
 *                 sharp or double flat it is "imaginary" (F-flat major
 *                 needs B double-flat)
 *   MINOR_RING    minor keys sit inside the circle beside the major key
 *                 with the same signature (A minor beside C major); the
 *                 circle's neighbor and enharmonic rules apply to them
 *                 too, so the bottom three signatures also pair G-sharp
 *                 and A-flat minor, D-sharp and E-flat minor, A-sharp
 *                 and B-flat minor
 *
 * A major key is named by its tonic: a letter and a semitone accidental
 * offset. The signed accidental count of a key is its place on the
 * line of fifths: positive is sharps, negative is flats (G is +1, F is
 * -1). It follows from the tonic alone, by 7 per accidental step.
 */

typedef enum {
    STANDARD_TONALITY_CIRCLE_RULE_ORDER = 0,
    STANDARD_TONALITY_CIRCLE_RULE_NEIGHBORS,
    STANDARD_TONALITY_CIRCLE_RULE_ENHARMONIC,
    STANDARD_TONALITY_CIRCLE_RULE_REAL_IMAGINARY,
    STANDARD_TONALITY_CIRCLE_RULE_MINOR_RING,
    STANDARD_TONALITY_CIRCLE_RULE_COUNT
} StandardTonalityCircleRule;

const Rule *standard_tonality_circle_rule(StandardTonalityCircleRule which);
const Constraint *standard_tonality_circle_constraint(StandardTonalityCircleRule which);

/* ORDER: signed accidental count of the major key with this tonic:
   +n for n sharps, -n for n flats, 0 for C. Beyond +-7 the key is
   imaginary. */
int standard_tonality_major_key_accidentals(PitchLetter letter, int accidental);

/* REAL_IMAGINARY: true when the key has at most 7 sharps or flats. */
bool standard_tonality_major_key_is_real(PitchLetter letter, int accidental);

/*
 * NEIGHBORS: the major key a fifth above (clockwise, one more sharp) or
 * a fifth below (counter-clockwise, one more flat) the given key. On true
 * *next_letter and *next_accidental name its tonic. False (outputs
 * untouched) for NULL outputs or when the neighbor's tonic would need
 * more than a double accidental.
 */
bool standard_tonality_circle_neighbor(PitchLetter letter, int accidental, bool clockwise, PitchLetter *next_letter,
                                       int *next_accidental);

/* ENHARMONIC: true when both keys are real, their tonics sound the same
   (12-TET), and they are named differently. */
bool standard_tonality_major_keys_enharmonic(PitchLetter letter_a, int accidental_a, PitchLetter letter_b,
                                             int accidental_b);

/* MINOR_RING / ENHARMONIC: true when both minor keys are real, their
   tonics sound the same (12-TET), and they are named differently. The
   fifth-neighbor step (standard_tonality_circle_neighbor) takes a minor
   tonic as it takes a major one. */
bool standard_tonality_minor_keys_enharmonic(PitchLetter letter_a, int accidental_a, PitchLetter letter_b,
                                             int accidental_b);

/*
 * The key signature of a real major key, with accidentals in the
 * conventional order (ORDER). False, leaving *out untouched, for an
 * imaginary key or on allocation failure. The caller destroys *out with
 * key_signature_destroy.
 */
bool standard_tonality_major_key_signature(PitchLetter letter, int accidental, KeySignature **out);

#endif
