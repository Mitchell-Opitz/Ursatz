#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_METER_CLASS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_METER_CLASS_RULES_H

#include <stdbool.h>

#include "theory/meter/grouping_rules.h"
#include "model/time/duration.h"
#include "model/time/note_value.h"
#include "model/time/time_signature.h"

/*
 * Meter class rules for standard-meter-v1: how a TimeSignature's two
 * numbers are read. A meter is simple or compound, and independently
 * duple, triple, or quadruple, giving six categories:
 *
 *   category           beats per group   beat divides into
 *   simple duple       2                 2
 *   simple triple      3                 2
 *   simple quadruple   4                 2
 *   compound duple     2                 3
 *   compound triple    3                 3
 *   compound quadruple 4                 3
 *
 * and the rules:
 *
 *   CLASSIFY            from the signature's beat groups (OMT I ch. 10),
 *                       the one owner of the beat: groups of three, two
 *                       or more, make a compound meter (6/8, 6/4, 12/8,
 *                       15/8), any other equal groups a simple one; the
 *                       number of groups gives duple, triple or
 *                       quadruple (none for 15/8, five beats). Unequal
 *                       groups (3+2) and a lone beat group -- 5/8, 7/8,
 *                       1/4 -- are unclassified: neither, not invalid,
 *                       because irregular meters follow other grouping
 *                       conventions.
 *   BEATS_PER_MEASURE   the number of beat groups: the beats of a
 *                       classified meter (6, 9, 12 over 8 are 2, 3, 4).
 *   BEAT_DIVISION       a simple beat divides into 2, then 4; a compound
 *                       beat divides into 3, then 6.
 *   COMPOUND_BEAT       because a compound beat holds three divisions it
 *                       is always dotted: a dotted quarter when the
 *                       bottom number is 8, a dotted half for 4, a dotted
 *                       eighth for 16 (three divisions of the bottom
 *                       unit). A simple beat is the bottom unit itself.
 *   COMPOUND_BOTTOM     the bottom number of a compound meter is usually
 *                       4, 8, or 16; any bottom number 1-16 is valid.
 *
 * A top number of 3 stays a simple triple (three groups of one).
 */

typedef enum {
    METER_TYPE_UNCLASSIFIED,
    METER_TYPE_SIMPLE,
    METER_TYPE_COMPOUND
} MeterType;

typedef struct {
    MeterType type;
    MeterGrouping grouping; /* METER_GROUPING_NONE when unclassified */
} MeterClass;

/* CLASSIFY: the signature's meter type and grouping. A NULL signature is
   unclassified. */
MeterClass standard_meter_classify(const TimeSignature *time_signature);

/* CLASSIFY: the type alone. */
MeterType standard_meter_type(const TimeSignature *time_signature);

/* CLASSIFY: the six-category table. For a simple or compound type and a
   duple/triple/quadruple grouping, sets the beats per group and the
   parts a beat divides into. False, leaving the outputs untouched, for an
   unclassified type or METER_GROUPING_NONE. */
bool standard_meter_category(MeterType type, MeterGrouping grouping, unsigned *out_beats, unsigned *out_division);

/* BEATS_PER_MEASURE: beats in one measure. False, leaving *out_beats
   untouched, when unclassified. */
bool standard_meter_beats_per_measure(const TimeSignature *time_signature, unsigned *out_beats);

/* BEAT_DIVISION: parts a beat divides into (2 simple, 3 compound) and
   parts a division further subdivides into per beat (4 simple, 6
   compound). False, leaving the outputs untouched, when unclassified. */
bool standard_meter_beat_division(MeterType type, unsigned *out_division, unsigned *out_subdivision);

/*
 * COMPOUND_BEAT / simple beat: the exact length of one beat, the
 * signature's own (time_signature_beat_length): the bottom unit for
 * simple meter, three bottom units for compound. False, leaving *out
 * untouched, when unclassified.
 */
bool standard_meter_beat_duration(const TimeSignature *time_signature, Duration *out);

#endif
