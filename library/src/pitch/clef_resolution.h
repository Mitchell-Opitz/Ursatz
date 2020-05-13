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
 */

/* Position the clef's sign is anchored on: step 2*(line-1). */
int clef_reference_steps(const Clef *clef);

/* Letter and octave written at position under clef. */
void clef_pitch_at_position(const Clef *clef, const StaffPosition *position, PitchLetter *out_letter,
                            int *out_octave);

/* Staff position at which letter/octave is written under clef. */
StaffPositionResult clef_position_for_pitch(const Clef *clef, PitchLetter letter, int octave,
                                            StaffPosition **out);

#endif
