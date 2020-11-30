#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_MINOR_SCALE_DEGREE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_MINOR_SCALE_DEGREE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "interpretation/scale_degree.h"
#include "minor_scale_rules.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Minor scale degree rules for standard-tonality-v1. Degrees are the
 * same numbers as in major (1 to 7, 8 for the closing tonic); solfege
 * and names follow the scale's form (minor_scale_rules.h):
 *
 *   SOLFEGE    natural: do re me fa sol le te. Harmonic: te becomes ti.
 *              Melodic ascending: le te become la ti. Melodic
 *              descending is natural again
 *   NAMES      as in major, except degree 7: te is the subtonic, ti the
 *              leading tone
 *   SUBTONIC   the subtonic is a whole step below the tonic (the
 *              leading tone is a half step below)
 *   LOWERED    degrees lowered versus the major scale on the same
 *              tonic: natural 3 6 7, harmonic 3 6, melodic ascending 3,
 *              melodic descending 3 6 7
 *
 * The degree numbers themselves carry no raised or lowered mark here;
 * the form says which pitch a degree is.
 */

typedef enum {
    STANDARD_TONALITY_MINOR_DEGREE_RULE_SOLFEGE = 0,
    STANDARD_TONALITY_MINOR_DEGREE_RULE_NAMES,
    STANDARD_TONALITY_MINOR_DEGREE_RULE_SUBTONIC,
    STANDARD_TONALITY_MINOR_DEGREE_RULE_LOWERED,
    STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT
} StandardTonalityMinorDegreeRule;

const Rule *standard_tonality_minor_degree_rule(StandardTonalityMinorDegreeRule which);
const Constraint *standard_tonality_minor_degree_constraint(StandardTonalityMinorDegreeRule which);

/* SOLFEGE: the syllable of degree 1..8 (8 is do) in the form. NULL for
   an invalid form or degree. */
const char *standard_tonality_minor_solfege(StandardTonalityMinorForm form, int degree);

/* NAMES: the degree's name in the form: "tonic" "supertonic" "mediant"
   "subdominant" "dominant" "submediant", then "subtonic" or "leading
   tone" for degree 7, and "tonic" for 8. NULL for an invalid form or
   degree. */
const char *standard_tonality_minor_degree_name(StandardTonalityMinorForm form, int degree);

/* SUBTONIC: true when `note` is a whole step below `tonic` (12-TET
   semitone distance of exactly 2). False for NULL. */
bool standard_tonality_is_subtonic(const PitchSpelling *tonic, const PitchSpelling *note);

/* LOWERED: writes the degrees lowered versus major in the form, in
   ascending order, to out (room for 3) and returns how many (3, 2, or
   1). 0 for an invalid form or NULL out. */
size_t standard_tonality_minor_lowered_degrees(StandardTonalityMinorForm form, int out[3]);

/* LOWERED: true when degree is one of the form's lowered degrees. */
bool standard_tonality_minor_degree_is_lowered(StandardTonalityMinorForm form, int degree);

/* The degree 1..7 `note` has in the minor scale of the form on `tonic`,
   whatever the octave of note (same letter and accidental required); 0
   when it is not in that scale, the tonic has no such scale, or an
   argument is NULL or invalid. For a descending form `tonic` is the
   lower tonic. */
int standard_tonality_minor_scale_degree_of(StandardTonalityMinorForm form, const PitchSpelling *tonic,
                                            const PitchSpelling *note);

/*
 * Builds a real, populated ScaleDegree claiming `pitch` is the given
 * degree (1..8) of the Scale named by scale_target_id, labeled with the
 * degree's name in the form. Same ownership as
 * standard_tonality_build_scale_degree (scale_degree_rules.h).
 */
ScaleDegreeResult standard_tonality_build_minor_scale_degree(StandardTonalityMinorForm form, int degree,
                                                             EntityID *scale_target_id, PitchIdentity *pitch,
                                                             ScaleDegree **out);

#endif
