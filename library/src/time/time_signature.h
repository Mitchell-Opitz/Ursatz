#ifndef MUSIC_TIME_TIME_SIGNATURE_H
#define MUSIC_TIME_TIME_SIGNATURE_H

#include <stdbool.h>

#include "time/note_value.h"

/*
 * TimeSignature (meter signature): how many beats a measure holds and
 * which note value is the beat. Written as two numbers, one above the
 * other, after the clef: 3/4 is three beats per measure with the quarter
 * note as the beat. It is not a fraction -- it names a beat count and a
 * beat unit, not a ratio -- so 2/4 and 4/8 are different signatures even
 * though a measure of each has the same length.
 *
 * beats is the top number, 1 or more (capped at TIME_SIGNATURE_MAX_BEATS
 * to keep the exact rationals bounded). The beat unit is the bottom
 * number: an undotted NoteValue from the whole note (1) to the sixteenth
 * note (16). A dotted beat unit is compound meter and is out of scope
 * here, so a dotted NoteValue is rejected rather than assumed.
 *
 * This type makes no claim about whether the meter is simple, compound,
 * duple, triple, or quadruple. What a beat count and unit mean as a
 * meter is frameworks/standard-meter/.
 *
 * Value type: immutable, value equality on (beats, beat unit).
 */

typedef struct TimeSignature TimeSignature;

#define TIME_SIGNATURE_MAX_BEATS 64
#define TIME_SIGNATURE_MIN_BEAT_UNIT_EXPONENT 0 /* whole note, bottom number 1 */
#define TIME_SIGNATURE_MAX_BEAT_UNIT_EXPONENT 4 /* sixteenth note, bottom number 16 */

typedef enum {
    TIME_SIGNATURE_OK = 0,
    TIME_SIGNATURE_ERR_NULL_BEAT_UNIT,
    TIME_SIGNATURE_ERR_BEATS_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_BEAT_UNIT_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_BEAT_UNIT_DOTTED,
    TIME_SIGNATURE_ERR_ALLOC
} TimeSignatureResult;

/* beat_unit is read, not owned: the caller still destroys it. */
TimeSignatureResult time_signature_create(unsigned beats, const NoteValue *beat_unit, TimeSignature **out);
void time_signature_destroy(TimeSignature *time_signature);

/* The top number. */
unsigned time_signature_beats(const TimeSignature *time_signature);

/* The beat unit as a NoteValue exponent (whole 0, half 1, quarter 2, ...). */
int time_signature_beat_unit_exponent(const TimeSignature *time_signature);

/* The bottom number: 1, 2, 4, 8, or 16. */
unsigned time_signature_beat_unit_number(const TimeSignature *time_signature);

bool time_signature_equals(const TimeSignature *a, const TimeSignature *b);

#endif
