#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_SPACING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_SPACING_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "semantics/voice_part.h"
#include "voicing_chord.h"

/*
 * Spacing rules (STANDARD_VOICING_RULE_SPACING). There is no more than an
 * octave between adjacent upper voices (soprano and alto, alto and
 * tenor) and no more than a twelfth between the tenor and the bass. The
 * most common error is between the alto and tenor, which are written in
 * different clefs.
 *
 * Distances are generic sizes (voicing_pitch.h): C4 to C5 is an octave,
 * C4 to C-sharp 5 is also a (generic) octave. The limits are 8 and 12.
 *
 * Spacing measures the distance only. A voice that has crossed below its
 * neighbor is a voice crossing error (crossing_rules.h), not a spacing
 * one; its distance is measured the same way.
 */

#define STANDARD_VOICING_UPPER_SPACING_LIMIT 8
#define STANDARD_VOICING_LOWER_SPACING_LIMIT 12

/* The largest generic size allowed between `upper` and the voice just
   below it: 8 for soprano and alto, 12 for tenor. 0 for the bass (nothing
   below it) or an invalid part. */
int standard_voicing_spacing_limit(VoicePart upper);

/* True when the two notes are within the limit for the pair of parts
   `upper` and the voice just below it. False for NULL, or a part with no
   voice below it. */
bool standard_voicing_spacing_valid(VoicePart upper, const PitchSpelling *upper_pitch,
                                    const PitchSpelling *lower_pitch);

/* The part whose spacing to the voice below is too wide, or
   VOICE_PART_COUNT when all three adjacent pairs are within limits.
   Reports the highest such part (soprano before alto before tenor).
   VOICE_PART_COUNT for NULL as well; use standard_voicing_chord_spacing_valid
   to tell the two apart. */
VoicePart standard_voicing_first_wide_spacing(const StandardVoicingChord *chord);

/* True when all three adjacent pairs are within limits. False for NULL. */
bool standard_voicing_chord_spacing_valid(const StandardVoicingChord *chord);

#endif
