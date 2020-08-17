#ifndef MUSIC_TIME_NOTE_VALUE_H
#define MUSIC_TIME_NOTE_VALUE_H

#include <stdbool.h>

#include "time/duration.h"

/*
 * NoteValue: the written rhythmic value of a note or rest -- whole, half,
 * quarter, and so on -- held as a power-of-two exponent: breve = -1,
 * whole = 0, half = 1, quarter = 2, eighth = 3, sixteenth = 4, and each
 * further step halves the value again. The range is breve through
 * 1024th (exponent -1 to 10).
 *
 * A NoteValue is notation vocabulary, not a magnitude: it converts to a
 * Duration (note_value_duration) but a Duration does not convert back,
 * because many written values can have the same length (a half note is
 * a dotted quarter plus an eighth, tied or not). Names, flags, and
 * glyph conventions are not held here; they are
 * frameworks/standard-rhythm/.
 *
 * A NoteValue may carry dots (up to NOTE_VALUE_MAX_DOTS). A dot adds
 * half the written value, and each further dot adds half the previous
 * dot's amount: a double-dotted quarter is 1 + 1/2 + 1/4 = 7/4 of a
 * quarter. The cap keeps the exact rational denominators small; no
 * written music uses more.
 *
 * Value type: immutable, value equality on (exponent, dots).
 */

typedef struct NoteValue NoteValue;

#define NOTE_VALUE_MIN_EXPONENT (-1)
#define NOTE_VALUE_MAX_EXPONENT 10
#define NOTE_VALUE_MAX_DOTS 4

typedef enum {
    NOTE_VALUE_OK = 0,
    NOTE_VALUE_ERR_EXPONENT_OUT_OF_RANGE,
    NOTE_VALUE_ERR_TOO_MANY_DOTS,
    NOTE_VALUE_ERR_ALLOC
} NoteValueResult;

/* An undotted value. */
NoteValueResult note_value_create(int exponent, NoteValue **out);
NoteValueResult note_value_create_dotted(int exponent, unsigned dots, NoteValue **out);
void note_value_destroy(NoteValue *note_value);

int note_value_exponent(const NoteValue *note_value);
unsigned note_value_dots(const NoteValue *note_value);

/* The exact length of this value: 2^-exponent of a whole note, times
   (2 - 2^-dots) for the dots. */
DurationResult note_value_duration(const NoteValue *note_value, Duration **out);

bool note_value_equals(const NoteValue *a, const NoteValue *b);

#endif
