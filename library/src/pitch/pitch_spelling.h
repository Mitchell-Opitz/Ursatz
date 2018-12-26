#ifndef MUSIC_PITCH_PITCH_SPELLING_H
#define MUSIC_PITCH_PITCH_SPELLING_H

#include <stdbool.h>

/*
 * PitchSpelling: a written pitch name -- letter, accidental, and octave.
 * Value equality on the (letter, accidental, octave) triple; enharmonic
 * equivalents (e.g. C# and Db) are intentionally NOT value-equal even
 * though they may sound identical under some tunings.
 *
 * accidental is treated opaquely here: its exact model is deferred to
 * Deliverable D. This layer only requires it be an integer offset (e.g.
 * -2 = double flat, -1 = flat, 0 = natural, 1 = sharp, 2 = double sharp)
 * and compares it by value, without interpreting it further.
 *
 * There is no sounding-pitch representation at this layer
 * (PitchRealization is L3+/Deliverable D) -- letter/accidental/octave are
 * always explicit and MUST NOT be reconstructed from one on deserialize.
 */

typedef enum {
    PITCH_LETTER_A,
    PITCH_LETTER_B,
    PITCH_LETTER_C,
    PITCH_LETTER_D,
    PITCH_LETTER_E,
    PITCH_LETTER_F,
    PITCH_LETTER_G
} PitchLetter;

typedef struct PitchSpelling PitchSpelling;

typedef enum {
    PITCH_SPELLING_OK = 0,
    PITCH_SPELLING_ERR_INVALID_LETTER,
    PITCH_SPELLING_ERR_ALLOC
} PitchSpellingResult;

PitchSpellingResult pitch_spelling_create(PitchLetter letter, int accidental, int octave, PitchSpelling **out);
void pitch_spelling_destroy(PitchSpelling *spelling);

/* Value equality on (letter, accidental, octave). Enharmonic equivalents
   are NOT equal. */
bool pitch_spelling_equals(const PitchSpelling *a, const PitchSpelling *b);

PitchLetter pitch_spelling_letter(const PitchSpelling *spelling);
int pitch_spelling_accidental(const PitchSpelling *spelling);
int pitch_spelling_octave(const PitchSpelling *spelling);

#endif