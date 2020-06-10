#ifndef MUSIC_PITCH_ACCIDENTAL_H
#define MUSIC_PITCH_ACCIDENTAL_H

#include <stdbool.h>

/*
 * Accidental: a notated sign that changes the pitch of a note. The five
 * kinds are double flat, flat, natural, sharp, and double sharp, with
 * semitone offsets -2, -1, 0, +1, and +2. A natural cancels a previous
 * accidental, so it carries offset 0; it is still a distinct kind from
 * "no accidental written".
 *
 * The offset is the integer PitchSpelling already uses for its
 * accidental field (pitch/pitch_spelling.h), so Accidental bridges to
 * it without changing PitchSpelling: accidental_semitone_offset gives the
 * value to store, accidental_from_offset recovers the sign. "Semitone"
 * is a unit of the offset only -- how many semitones make an octave, or
 * what a semitone sounds like, is a tuning matter and is not defined
 * here (see TuningSystem). Glyph variants (double sharp as the cross
 * symbol or as two sharps) are presentation, not meaning, and are not
 * modeled.
 *
 * Value type: immutable, value equality on kind.
 */

typedef enum {
    ACCIDENTAL_DOUBLE_FLAT,
    ACCIDENTAL_FLAT,
    ACCIDENTAL_NATURAL,
    ACCIDENTAL_SHARP,
    ACCIDENTAL_DOUBLE_SHARP
} AccidentalKind;

typedef struct Accidental Accidental;

typedef enum {
    ACCIDENTAL_OK = 0,
    ACCIDENTAL_ERR_INVALID_KIND,
    ACCIDENTAL_ERR_INVALID_OFFSET,
    ACCIDENTAL_ERR_ALLOC
} AccidentalResult;

#define ACCIDENTAL_MIN_OFFSET (-2)
#define ACCIDENTAL_MAX_OFFSET 2

AccidentalResult accidental_create(AccidentalKind kind, Accidental **out);

/* Offset -2..+2 -> the matching kind; anything else is
   ACCIDENTAL_ERR_INVALID_OFFSET. */
AccidentalResult accidental_from_offset(int semitone_offset, Accidental **out);
void accidental_destroy(Accidental *accidental);

AccidentalKind accidental_kind(const Accidental *accidental);

/* -2, -1, 0, +1, or +2. */
int accidental_semitone_offset(const Accidental *accidental);

/* Value equality on kind. */
bool accidental_equals(const Accidental *a, const Accidental *b);

#endif
