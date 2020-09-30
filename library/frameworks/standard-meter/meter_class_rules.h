#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_METER_CLASS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_METER_CLASS_RULES_H

#include <stdbool.h>

#include "grouping_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/note_value.h"
#include "time/time_signature.h"

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
 *   CLASSIFY            top number 2, 3, 4 is simple (duple, triple,
 *                       quadruple); 6, 9, 12 is compound (duple, triple,
 *                       quadruple). Any other top number -- 5/8, 7/8 --
 *                       is unclassified: neither, not invalid, because
 *                       TimeSignature allows it and irregular meters
 *                       follow other grouping conventions.
 *   BEATS_PER_MEASURE   simple: the top number counts beats. Compound:
 *                       the top number counts DIVISIONS, and divided by
 *                       three gives the beats (6, 9, 12 are 2, 3, 4).
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
 * The compound top number is "a multiple of three" only in the 6, 9, 12
 * cases that give duple, triple, and quadruple; 3 stays a simple triple
 * and 15, 18, ... are unclassified.
 */

typedef enum {
    STANDARD_METER_CLASS_RULE_CLASSIFY = 0,
    STANDARD_METER_CLASS_RULE_BEATS_PER_MEASURE,
    STANDARD_METER_CLASS_RULE_BEAT_DIVISION,
    STANDARD_METER_CLASS_RULE_COMPOUND_BEAT,
    STANDARD_METER_CLASS_RULE_COMPOUND_BOTTOM,
    STANDARD_METER_CLASS_RULE_COUNT
} StandardMeterClassRule;

typedef enum {
    METER_TYPE_UNCLASSIFIED,
    METER_TYPE_SIMPLE,
    METER_TYPE_COMPOUND
} MeterType;

typedef struct {
    MeterType type;
    MeterGrouping grouping; /* METER_GROUPING_NONE when unclassified */
} MeterClass;

const Rule *standard_meter_class_rule(StandardMeterClassRule which);
const Constraint *standard_meter_class_constraint(StandardMeterClassRule which);

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
 * COMPOUND_BEAT / simple beat: the exact length of one beat: the bottom
 * unit for simple meter, three bottom units for compound. False, leaving
 * *out untouched, when unclassified or on allocation failure. On success
 * the caller owns *out.
 */
bool standard_meter_beat_duration(const TimeSignature *time_signature, Duration **out);

/*
 * COMPOUND_BEAT: the beat as a written value -- the bottom unit for
 * simple meter, the dotted note one step longer for compound (a dotted
 * quarter in 6/8). False, leaving *out untouched, when unclassified or on
 * allocation failure. On success the caller owns *out.
 */
bool standard_meter_beat_note_value(const TimeSignature *time_signature, NoteValue **out);

/* COMPOUND_BOTTOM: true when the signature is compound and its bottom
   number is 4, 8, or 16. */
bool standard_meter_compound_bottom_is_usual(const TimeSignature *time_signature);

#endif
