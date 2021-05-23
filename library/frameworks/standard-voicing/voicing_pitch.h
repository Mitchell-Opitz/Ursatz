#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_PITCH_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_PITCH_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * Pitch arithmetic for standard-voicing-v1.
 *
 * ASSUMPTION: twelve semitones to the octave (12-tone equal
 * temperament), the same layout standard-notation's semitone.h and
 * standard-tonality's tonal_pitch.h use. It is a property of this
 * framework, not of src/pitch/. This module carries its own copy rather
 * than depending on another framework: framework modules do not depend
 * on each other.
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
 * of the voice (an SATB voicing is written at concert pitch).
 */

#define STANDARD_VOICING_SEMITONES_PER_OCTAVE 12

/* spelling must not be NULL. */
int standard_voicing_semitone_number(const PitchSpelling *spelling);

/* Diatonic step number of the letter and octave, accidental ignored:
   7 * octave + (C=0 D=1 E=2 F=3 G=4 A=5 B=6). spelling must not be NULL. */
int standard_voicing_step_number(const PitchSpelling *spelling);

/* The generic size (unison 1, second 2, ... octave 8, twelfth 12) from
   the lower of the two notes up to the higher, accidentals and direction
   ignored. Both must not be NULL. */
int standard_voicing_generic_size(const PitchSpelling *a, const PitchSpelling *b);

/* True when both have the same letter and accidental, whatever the
   octave. Enharmonic spellings are not the same (C-sharp, D-flat). False
   for NULL. */
bool standard_voicing_same_pitch_class(const PitchSpelling *a, const PitchSpelling *b);

#endif
