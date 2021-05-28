#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_PITCH_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_PITCH_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * Pitch arithmetic for standard-counterpoint-v1.
 *
 * ASSUMPTION: twelve semitones to the octave (12-tone equal
 * temperament), the same layout standard-notation's semitone.h,
 * standard-tonality's tonal_pitch.h, and standard-voicing's
 * voicing_pitch.h use. It is a property of this framework, not of
 * src/pitch/. This module carries its own copy rather than depending on
 * another framework: framework modules do not depend on each other.
 *
 * A pitch's semitone number is
 *
 *   12 * octave + letter_base + accidental
 *
 * with letter_base C=0, D=2, E=4, F=5, G=7, A=9, B=11. The octave number
 * belongs to the LETTER, not the sounding register (B-sharp in octave 3
 * has the number of C in octave 4).
 *
 * Every PitchSpelling given to this module is read as the SOUNDING pitch
 * of the part.
 */

#define STANDARD_COUNTERPOINT_SEMITONES_PER_OCTAVE 12

/* spelling must not be NULL. */
int standard_counterpoint_semitone_number(const PitchSpelling *spelling);

/* Diatonic step number of the letter and octave, accidental ignored:
   7 * octave + (C=0 D=1 E=2 F=3 G=4 A=5 B=6). spelling must not be NULL. */
int standard_counterpoint_step_number(const PitchSpelling *spelling);

/* The direction of motion from `from` to `to`: 1 up, -1 down, 0 for the
   same sounding pitch. Compares semitone numbers first; spellings that
   sound alike are told apart by step number (B-sharp 3 to C4 is up).
   Neither may be NULL. */
int standard_counterpoint_direction(const PitchSpelling *from, const PitchSpelling *to);

/* True when both have the same letter, accidental, and octave. False for
   NULL. */
bool standard_counterpoint_same_pitch(const PitchSpelling *a, const PitchSpelling *b);

#endif
