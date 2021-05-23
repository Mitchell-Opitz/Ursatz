#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_CROSSING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_CROSSING_RULES_H

#include <stdbool.h>

#include "semantics/voice_part.h"
#include "voicing_chord.h"

/*
 * Voice crossing rules (STANDARD_VOICING_RULE_VOICE_CROSSING). The ranges
 * of the voices do not cross: the soprano is always higher than the
 * alto, the alto higher than the tenor, the tenor higher than the bass.
 * The most common error is between the alto and tenor.
 *
 * "Higher" is strict, by sounding pitch (the semitone number in
 * voicing_pitch.h): two adjacent voices on the same pitch are not a
 * correctly ordered pair.
 */

/* True when `upper` sounds strictly higher than `lower`. False for NULL. */
bool standard_voicing_pair_ordered(const PitchSpelling *upper, const PitchSpelling *lower);

/* The part that sounds at or below the voice just under it, or
   VOICE_PART_COUNT when all three adjacent pairs are in order (soprano
   checked first). VOICE_PART_COUNT for NULL as well; use
   standard_voicing_chord_uncrossed to tell the two apart. */
VoicePart standard_voicing_first_crossing(const StandardVoicingChord *chord);

/* True when soprano > alto > tenor > bass. False for NULL. */
bool standard_voicing_chord_uncrossed(const StandardVoicingChord *chord);

#endif
