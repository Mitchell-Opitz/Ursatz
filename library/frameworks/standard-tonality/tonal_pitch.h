#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TONAL_PITCH_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TONAL_PITCH_H

#include "pitch/pitch_spelling.h"

/*
 * Pitch arithmetic for standard-tonality-v1.
 *
 * ASSUMPTION: twelve semitones to the octave (12-tone equal
 * temperament), the same layout standard-notation's semitone.h uses. It
 * is a property of this framework, not of src/pitch/, which takes no
 * tuning assumptions. This module carries its own copy rather than
 * depending on standard-notation: framework modules do not depend on
 * each other.
 *
 * A pitch's semitone number is
 *
 *   12 * octave + letter_base + accidental
 *
 * with letter_base C=0, D=2, E=4, F=5, G=7, A=9, B=11. The octave number
 * belongs to the LETTER, not the sounding register (B-sharp in octave 3
 * has the number of C in octave 4).
 */

#define STANDARD_TONALITY_SEMITONES_PER_OCTAVE 12

/* Semitones above C of the natural letter: C=0 ... B=11. */
int standard_tonality_letter_semitone(PitchLetter letter);

/* spelling must not be NULL. */
int standard_tonality_semitone_number(const PitchSpelling *spelling);

/* The letter after `letter` in the loop: C D E F G A B C. */
PitchLetter standard_tonality_next_letter(PitchLetter letter);

#endif
