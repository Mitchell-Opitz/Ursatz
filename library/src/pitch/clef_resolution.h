#ifndef MUSIC_PITCH_CLEF_RESOLUTION_H
#define MUSIC_PITCH_CLEF_RESOLUTION_H

#include "pitch/clef.h"
#include "pitch/pitch_spelling.h"
#include "pitch/staff_position.h"

/*
 * Clef resolution: the mapping between a StaffPosition and a diatonic
 * pitch (letter + octave) under a Clef. Each step on the staff is one
 * diatonic step (line to adjacent space, space to adjacent line), so the
 * clef's reference pitch sits at the position of its anchor line
 * (line N -> step 2*(N-1)) and every other position is counted from there.
 *
 * Accidentals are not part of staff placement and play no role here --
 * letter and octave only. The two functions are exact inverses.
 *
 * Every function refuses an unpitched (neutral) clef with
 * CLEF_RESOLUTION_ERR_UNPITCHED and leaves its outputs untouched.
 */

typedef enum {
    CLEF_RESOLUTION_OK = 0,
    CLEF_RESOLUTION_ERR_UNPITCHED, /* neutral clef: no pitches to resolve */
    CLEF_RESOLUTION_ERR_ALLOC
} ClefResolutionResult;

/* Position the clef's sign is anchored on: step 2*(line-1). */
ClefResolutionResult clef_reference_steps(const Clef *clef, int *out_steps);

/* Letter and octave written at position under clef. */
ClefResolutionResult clef_pitch_at_position(const Clef *clef, const StaffPosition *position,
                                            PitchLetter *out_letter, int *out_octave);

/*
 * Letter and octave that SOUND for the note written at position: the
 * written pitch moved by the clef's octave shift (an octave treble clef
 * with an "8" below sounds an octave lower than written). Letter is
 * unchanged. Instrument-level conventions such as piccolo always sounding
 * an octave higher are not applied -- those belong to Instrument.
 */
ClefResolutionResult clef_sounding_pitch_at_position(const Clef *clef, const StaffPosition *position,
                                                     PitchLetter *out_letter, int *out_octave);

/* Staff position at which letter/octave is written under clef. */
ClefResolutionResult clef_position_for_pitch(const Clef *clef, PitchLetter letter, int octave,
                                             StaffPosition **out);

#endif
