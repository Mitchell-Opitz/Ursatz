#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_RANGE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_RANGE_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "semantics/voice_part.h"
#include "voicing_chord.h"

/*
 * Range rules (STANDARD_VOICING_RULE_RANGE). Each voice has a generally
 * accepted range, both ends included:
 *
 *   soprano  C4 to G5      alto  G3 to D5
 *   tenor    C3 to G4      bass  F2 to D4
 *
 * A pitch is measured by its semitone number (voicing_pitch.h), so an
 * enharmonic spelling sounding inside the range is in range (B-sharp 3 is
 * C4); the written letter and octave of a pitch are not what is
 * compared. The rules take the sounding pitch.
 */

/* The lowest and highest semitone numbers of the part's range. False
   (outputs untouched) for an invalid part or a NULL output. */
bool standard_voicing_range_semitones(VoicePart part, int *lowest, int *highest);

/* The lowest and highest notes of the part's range as spellings
   (C natural, G natural, etc.), written into caller-owned storage: out
   letters and octaves only, accidental 0. False for an invalid part or
   NULL. */
bool standard_voicing_range_bounds(VoicePart part, PitchLetter *lowest_letter, int *lowest_octave,
                                   PitchLetter *highest_letter, int *highest_octave);

/* True when the pitch is inside the part's range. False for NULL or an
   invalid part. */
bool standard_voicing_in_range(VoicePart part, const PitchSpelling *pitch);

/* True when every voice of the chord is in its range. False for NULL. */
bool standard_voicing_chord_in_range(const StandardVoicingChord *chord);

#endif
