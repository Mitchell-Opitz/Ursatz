#ifndef MUSIC_TIME_TIME_SIGNATURE_H
#define MUSIC_TIME_TIME_SIGNATURE_H

#include <stdbool.h>

#include "time/note_value.h"

/*
 * TimeSignature (meter signature): the two numbers written one above the
 * other after the clef, such as 3/4 or 6/8. This type holds exactly
 * those two numbers and nothing more: the top number (a count) and the
 * bottom number (a note value). It is not a fraction -- it is a pair of
 * numbers, not a ratio -- so 2/4 and 4/8 are different signatures even
 * though a measure of each has the same length.
 *
 * What the numbers MEAN depends on the meter. In a simple meter the top
 * number counts beats and the bottom is the beat unit; in a compound
 * meter the top number counts divisions and the bottom is the division
 * unit (6/8 is two beats of three eighth notes, not six beats). This
 * type takes no side: it makes no claim about beats, or about whether the
 * meter is simple, compound, duple, triple, or quadruple. Those readings
 * are frameworks/standard-meter/.
 *
 * The top number is 1 or more (capped at TIME_SIGNATURE_MAX_TOP_NUMBER
 * to keep the exact rationals bounded). The bottom number is an undotted
 * NoteValue from the whole note (1) to the sixteenth note (16): a dotted
 * NoteValue is rejected. A measure's length is the top number times the
 * bottom unit's length whatever the meter, so that is held here.
 *
 * Value type: immutable, value equality on (top number, bottom unit).
 */

typedef struct TimeSignature TimeSignature;

#define TIME_SIGNATURE_MAX_TOP_NUMBER 64
#define TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT 0 /* whole note, bottom number 1 */
#define TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT 4 /* sixteenth note, bottom number 16 */

typedef enum {
    TIME_SIGNATURE_OK = 0,
    TIME_SIGNATURE_ERR_NULL_BOTTOM_UNIT,
    TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_BOTTOM_UNIT_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_BOTTOM_UNIT_DOTTED,
    TIME_SIGNATURE_ERR_ALLOC
} TimeSignatureResult;

/* bottom_unit is read, not owned: the caller still destroys it. */
TimeSignatureResult time_signature_create(unsigned top_number, const NoteValue *bottom_unit, TimeSignature **out);
void time_signature_destroy(TimeSignature *time_signature);

/* The top number. */
unsigned time_signature_top_number(const TimeSignature *time_signature);

/* The bottom unit as a NoteValue exponent (whole 0, half 1, quarter 2, ...). */
int time_signature_bottom_unit_exponent(const TimeSignature *time_signature);

/* The bottom number: 1, 2, 4, 8, or 16. */
unsigned time_signature_bottom_number(const TimeSignature *time_signature);

/* The exact length of one bottom unit: 1/bottom number of a whole note. */
DurationResult time_signature_unit_duration(const TimeSignature *time_signature, Duration **out);

/* The exact length of one measure: the top number times the unit
   duration. 4/4 and 2/2 are both 1, 3/4 is 3/4, and 6/8 is 3/4. */
DurationResult time_signature_measure_duration(const TimeSignature *time_signature, Duration **out);

bool time_signature_equals(const TimeSignature *a, const TimeSignature *b);

#endif
