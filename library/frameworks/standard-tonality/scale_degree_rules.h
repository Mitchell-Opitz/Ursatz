#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SCALE_DEGREE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SCALE_DEGREE_RULES_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "interpretation/scale_degree.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Scale degree rules for standard-tonality-v1. The notes of a major
 * scale are named three ways:
 *
 *   NUMBERS          scale degrees 1 to 7; the last note of the scale
 *                    is 1 again (degree 8 here names that repeat)
 *   SOLFEGE          movable-do solfege: do re mi fa sol la ti, with do
 *                    wherever the tonic is (fixed do, with do always C,
 *                    is a different system and not defined here)
 *   NAMES            tonic, supertonic, mediant, subdominant, dominant,
 *                    submediant, leading tone, then tonic again
 *   NAME_DERIVATION  the names come from generic intervals to the tonic:
 *                    super- is above, sub- is below. Supertonic is a
 *                    second above, mediant a third above (the middle
 *                    of tonic and dominant), dominant a fifth above;
 *                    subdominant is a fifth below, submediant a third
 *                    below
 *   LEADING_TONE     the leading tone is a half step below the tonic
 *
 * Degrees are 1..7, or 8 for the closing tonic.
 */

#define STANDARD_TONALITY_SCALE_DEGREE_COUNT 7
#define STANDARD_TONALITY_SCALE_DEGREE_CLOSING_TONIC 8

typedef enum {
    STANDARD_TONALITY_SCALE_DEGREE_RULE_NUMBERS = 0,
    STANDARD_TONALITY_SCALE_DEGREE_RULE_SOLFEGE,
    STANDARD_TONALITY_SCALE_DEGREE_RULE_NAMES,
    STANDARD_TONALITY_SCALE_DEGREE_RULE_NAME_DERIVATION,
    STANDARD_TONALITY_SCALE_DEGREE_RULE_LEADING_TONE,
    STANDARD_TONALITY_SCALE_DEGREE_RULE_COUNT
} StandardTonalityScaleDegreeRule;

const Rule *standard_tonality_scale_degree_rule(StandardTonalityScaleDegreeRule which);
const Constraint *standard_tonality_scale_degree_constraint(StandardTonalityScaleDegreeRule which);

/* SOLFEGE: "do" "re" "mi" "fa" "sol" "la" "ti" for 1..7, "do" for 8.
   NULL for any other value. */
const char *standard_tonality_solfege(int degree);

/* NAMES: "tonic" "supertonic" "mediant" "subdominant" "dominant"
   "submediant" "leading tone" for 1..7, "tonic" for 8. NULL for any
   other value. */
const char *standard_tonality_degree_name(int degree);

/* NAME_DERIVATION: for degrees 2, 3, 5 (above the tonic) and 4, 6 (below
   it), sets *generic_interval to 2, 3, 5, 5, 3 and *below to false,
   false, false, true, true, and returns true. False (outputs untouched)
   for 1, 7, 8, anything else, or NULL outputs. */
bool standard_tonality_degree_derivation(int degree, int *generic_interval, bool *below);

/* LEADING_TONE: true when `note` is a half step below `tonic` (12-TET
   semitone distance of exactly 1, octave aside from the numbering). False
   for NULL. */
bool standard_tonality_is_leading_tone(const PitchSpelling *tonic, const PitchSpelling *note);

/* NUMBERS: the degree 1..7 `note` has in the major scale on `tonic`,
   whatever the octave of note; 0 when note is not in that scale (same
   letter and accidental required) or tonic has no scale (see
   standard_tonality_build_major_scale). 0 for NULL. */
int standard_tonality_scale_degree_of(const PitchSpelling *tonic, const PitchSpelling *note);

/*
 * Builds a real, populated ScaleDegree claiming that `pitch` is the
 * given degree of the Scale named by scale_target_id, under
 * standard-tonality-v1. Its label is the degree's name
 * (standard_tonality_degree_name). degree is 1..8.
 *
 * Takes ownership of scale_target_id and pitch on success (destroyed
 * transitively by scale_degree_destroy(*out)); on failure the caller
 * still owns them.
 */
ScaleDegreeResult standard_tonality_build_scale_degree(int degree, EntityID *scale_target_id, PitchIdentity *pitch,
                                                       ScaleDegree **out);

#endif
