#ifndef MUSIC_TIME_TIME_SIGNATURE_H
#define MUSIC_TIME_TIME_SIGNATURE_H

#include <stdbool.h>
#include <stddef.h>

#include "model/time/note_value.h"

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
 * Beat groups (R-587): a signature also holds the groups its units fall
 * into, the beats of a bar counted in bottom units. The top number is their
 * sum. time_signature_create gives the default groups: a top number above 3
 * that 3 divides is groups of three (6/8 is 3+3, 6/4 is 3+3, 12/8 is
 * 3+3+3+3); any other top number is one unit per group (4/4 is 1+1+1+1, 5/8
 * is 1+1+1+1+1). time_signature_create_grouped takes the groups as given
 * (3+2 over 8 for an additive 5/8). The signature is the one owner of the
 * beat: every reader of beats calls time_signature_beat_length.
 *
 * Value type: immutable, value equality on (groups, bottom unit).
 */

typedef struct TimeSignature TimeSignature;

#define TIME_SIGNATURE_MAX_TOP_NUMBER 64
#define TIME_SIGNATURE_MAX_GROUPS 16      /* of time_signature_create_grouped; the default groups of a top number above 16 are more */
#define TIME_SIGNATURE_MAX_GROUP_UNITS 16 /* bottom units in one group of time_signature_create_grouped */
#define TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT 0 /* whole note, bottom number 1 */
#define TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT 4 /* sixteenth note, bottom number 16 */

typedef enum {
    TIME_SIGNATURE_OK = 0,
    TIME_SIGNATURE_ERR_NULL_BOTTOM_UNIT,
    TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_BOTTOM_UNIT_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_BOTTOM_UNIT_DOTTED,
    TIME_SIGNATURE_ERR_NULL_SOURCE,
    TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE,
    TIME_SIGNATURE_ERR_ALLOC
} TimeSignatureResult;

/* bottom_unit is read, not owned: the caller still destroys it. */
TimeSignatureResult time_signature_create(unsigned top_number, const NoteValue *bottom_unit, TimeSignature **out);

/* The signature with the given beat groups: group_count (1 to TIME_SIGNATURE_MAX_GROUPS) groups of 1 to
   TIME_SIGNATURE_MAX_GROUP_UNITS bottom units each, in order; the top number is their sum, which the top number
   range holds too (else TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE). TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE for a
   count or a group outside those. bottom_unit is read, not owned. */
TimeSignatureResult time_signature_create_grouped(const unsigned *groups, size_t group_count, const NoteValue *bottom_unit,
                                                  TimeSignature **out);

/* An independent copy, groups included; TIME_SIGNATURE_ERR_NULL_SOURCE for a NULL source. */
TimeSignatureResult time_signature_copy(const TimeSignature *source, TimeSignature **out);

void time_signature_destroy(TimeSignature *time_signature);

/* The top number. */
unsigned time_signature_top_number(const TimeSignature *time_signature);

/* The bottom unit as a NoteValue exponent (whole 0, half 1, quarter 2, ...). */
int time_signature_bottom_unit_exponent(const TimeSignature *time_signature);

/* The bottom number: 1, 2, 4, 8, or 16. */
unsigned time_signature_bottom_number(const TimeSignature *time_signature);

/* The exact length of one bottom unit: 1/bottom number of a whole note. */
DurationResult time_signature_unit_duration(const TimeSignature *time_signature, Duration *out);

/* The exact length of one measure: the top number times the unit
   duration. 4/4 and 2/2 are both 1, 3/4 is 3/4, and 6/8 is 3/4. */
DurationResult time_signature_measure_duration(const TimeSignature *time_signature, Duration *out);

/* The number of beat groups, and the units in group `index` (0 for an index past the last). */
size_t time_signature_group_count(const TimeSignature *time_signature);
unsigned time_signature_group_at(const TimeSignature *time_signature, size_t index);

/* Whether the beat is a group of three units: two or more groups, all of three (6/8, 9/8, 12/8, 6/4, 15/8). The
   beat of such a meter is a dotted unit. */
bool time_signature_is_compound(const TimeSignature *time_signature);

/* The exact length of one beat, the one answer to "how long is a beat": the length of a group when every group
   has the same units (4/4 a quarter, 6/8 a dotted quarter, 6/4 a dotted half), else the bottom unit (3+2 over 8 has
   eighth-note beats, so every reader of beats reads an additive meter as it reads 5/8). */
DurationResult time_signature_beat_length(const TimeSignature *time_signature, Duration *out);

/* Value equality: the same groups over the same bottom unit (a NULL is equal to nothing). */
bool time_signature_equals(const TimeSignature *a, const TimeSignature *b);

#endif
