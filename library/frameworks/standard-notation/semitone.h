#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_SEMITONE_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_SEMITONE_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * Semitone numbering for standard-notation-v1.
 *
 * ASSUMPTION: this module numbers pitches in 12-tone equal temperament --
 * twelve semitones to the octave, the layout of the piano keyboard. That
 * is a property of this framework, not of src/pitch/, which deliberately
 * takes no tuning assumptions (docs/modules/pitch.md). It is isolated in
 * this file so it can be lifted into a shared module if a second
 * framework needs the same arithmetic.
 *
 * A pitch's semitone number is
 *
 *   12 * octave + letter_base + accidental
 *
 * with letter_base C=0, D=2, E=4, F=5, G=7, A=9, B=11. The octave number
 * belongs to the LETTER, not the sounding register, so B-sharp in octave 3
 * has the number of C in octave 4, and C-flat in octave 4 has the number
 * of B in octave 3. Enharmonic spellings therefore share a number
 * (pitch_spelling_equals still tells them apart).
 */

#define STANDARD_NOTATION_SEMITONES_PER_OCTAVE 12

/* Semitones above C of the natural letter: C=0 ... B=11. */
int standard_notation_letter_semitone(PitchLetter letter);

/* spelling must not be NULL. */
int standard_notation_semitone_number(const PitchSpelling *spelling);

/* Signed distance in semitones from `from` up to `to` (negative when
   `to` is lower). */
int standard_notation_semitone_distance(const PitchSpelling *from, const PitchSpelling *to);

/* Semitone number reduced to 0..11, with C = 0 (octave removed). */
int standard_notation_pitch_class(const PitchSpelling *spelling);

/* White keys of the keyboard are pitch classes 0, 2, 4, 5, 7, 9, 11; the
   rest are black keys. Both are false for NULL. */
bool standard_notation_is_white_key(const PitchSpelling *spelling);
bool standard_notation_is_black_key(const PitchSpelling *spelling);

#endif
