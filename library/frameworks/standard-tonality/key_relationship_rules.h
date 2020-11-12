#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_KEY_RELATIONSHIP_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_KEY_RELATIONSHIP_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Major/minor key relationship rules for standard-tonality-v1. A key is
 * named by its tonic (a letter and a semitone accidental offset) and
 * its mode; a minor key rests on the natural minor scale.
 *
 *   PARALLEL        a major and a minor key share a tonic (C major and
 *                   C minor; A-flat major and A-flat minor)
 *   RELATIVE        a major and a minor key share a key signature; the
 *                   minor tonic is three half steps below the major's
 *                   (C major and A minor)
 *   RELATIVE_SPELLING
 *                   relative keys have the same signature, so the
 *                   enharmonic choice matters: D-flat major's relative
 *                   minor is B-flat minor (five flats), not A-sharp
 *                   minor (seven sharps)
 *
 * The relative minor's tonic is the sixth degree of the major scale
 * (and the relative major's tonic the third degree of the natural minor
 * scale), which gives the spelling that keeps the signature. The
 * three-half-steps count alone cannot.
 */

typedef enum {
    STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_PARALLEL = 0,
    STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_RELATIVE,
    STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_RELATIVE_SPELLING,
    STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_COUNT
} StandardTonalityKeyRelationshipRule;

const Rule *standard_tonality_key_relationship_rule(StandardTonalityKeyRelationshipRule which);
const Constraint *standard_tonality_key_relationship_constraint(StandardTonalityKeyRelationshipRule which);

/* PARALLEL: true when the two tonics are the same name (letter and
   accidental). */
bool standard_tonality_keys_parallel(PitchLetter major_letter, int major_accidental, PitchLetter minor_letter,
                                     int minor_accidental);

/* RELATIVE_SPELLING: the tonic of the relative minor of a major key, as
   a letter and accidental. False (outputs untouched) for NULL outputs
   or a key whose scale needs more than a double accidental. */
bool standard_tonality_relative_minor(PitchLetter major_letter, int major_accidental, PitchLetter *minor_letter,
                                      int *minor_accidental);

/* RELATIVE_SPELLING: the tonic of the relative major of a minor key.
   Same failure conditions. */
bool standard_tonality_relative_major(PitchLetter minor_letter, int minor_accidental, PitchLetter *major_letter,
                                      int *major_accidental);

/* RELATIVE: true when the minor tonic is the relative minor's tonic of
   the major key, spelled correctly, and sounds three half steps below
   the major tonic. */
bool standard_tonality_keys_relative(PitchLetter major_letter, int major_accidental, PitchLetter minor_letter,
                                     int minor_accidental);

#endif
