#ifndef MUSIC_PITCH_PITCH_ARITHMETIC_H
#define MUSIC_PITCH_PITCH_ARITHMETIC_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_spelling.h"

/*
 * Spelling arithmetic: letters, semitones, pitch classes, MIDI numbers,
 * the line of fifths, and names. Theory-neutral in the sense that no key,
 * mode, or scale is involved; the one assumption is twelve semitones to
 * the octave (12-tone equal temperament), the layout MIDI uses. This
 * module takes no PitchSpelling by anything but value of its letter,
 * accidental, and octave, so enharmonic spellings stay distinct.
 *
 * Accidentals are integers (-2 = double flat ... +2 = double sharp), the
 * same integer PitchSpelling stores.
 */

#define PITCH_SEMITONES_PER_OCTAVE 12

/* Semitones above C of the natural letter: C=0 ... B=11. */
int pitch_letter_semitone(PitchLetter letter);

/* Position in the loop C D E F G A B: C=0 ... B=6. */
int pitch_letter_index(PitchLetter letter);

/* The letter `steps` places along C D E F G A B. Wraps both ways. */
PitchLetter pitch_letter_offset(PitchLetter letter, int steps);

/* The letter named by an upper-case character A to G. False (*out untouched) for any other character or a NULL out. */
bool pitch_letter_from_char(char c, PitchLetter *out);

/* Position in the loop of letters counted over octaves: 7 * octave + the letter's index (C=0 ... B=6), so C4 is 28. 0 for a NULL
 * spelling. Differences of these numbers are letter steps whatever the accidentals. */
int pitch_spelling_step_number(const PitchSpelling *spelling);

/* The generic size of the interval from a to b in letter steps, |step difference| + 1, so a unison is 1 and C4 to G4 is 5.
 * Direction and accidentals are ignored. 1 when either spelling is NULL. */
int pitch_spelling_generic_size(const PitchSpelling *a, const PitchSpelling *b);

/* 0 to 11 for a letter and accidental, whatever the octave. */
int pitch_class_of_letter(PitchLetter letter, int accidental);

/* Position on the line of fifths: C=0, G=1, D=2 ..., F=-1; each sharp adds 7, each flat
 * subtracts 7. */
int pitch_spelling_fifths(PitchLetter letter, int accidental);

/* The inverse of pitch_spelling_fifths. letter and accidental must not be NULL. */
void pitch_spelling_from_fifths(int fifths, PitchLetter *letter, int *accidental);

/*
 * Writes the name without octave: C, F#, Bb, Cx, Ebb. Accidentals -2 to +2
 * only. Returns false (buffer untouched) for an invalid letter, an accidental
 * outside that range, a NULL buffer, or a buffer too small for the name and its terminator.
 */
bool pitch_spelling_name(PitchLetter letter, int accidental, char *buffer, size_t size);

/*
 * Parses an upper-case letter, an optional accidental (# b x bb ##), and an
 * optional integer octave (-1, 4). The whole text must be consumed. If
 * octave_or_null is NULL, text carrying an octave is rejected. If the text has no octave,
 * *octave_or_null is left unchanged. On false, no output is written.
 */
bool pitch_spelling_parse_name(const char *text, PitchLetter *letter, int *accidental,
                               int *octave_or_null);

/* "C", "C#" or "Db" ... for 0 to 11; NULL outside that range. */
const char *pitch_class_name(int pitch_class, bool prefer_flats);

#endif
