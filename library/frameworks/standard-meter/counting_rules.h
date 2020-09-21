#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_COUNTING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_COUNTING_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "meter_class_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Counting rules for standard-meter-v1: American traditional counting,
 * the system Open Music Theory uses (others exist). A position in a
 * measure is counted relative to its beat, whatever the beat unit, so the
 * same rhythm in 4/4, 4/2, 4/8, and 4/16 is counted the same way -- and a
 * rhythm in 6/8, 6/4, and 6/16 is counted alike too:
 *
 *   BEATS         each beat is its Arabic numeral: "1 2 3 4"
 *   DIVISIONS     simple: halfway through a beat is "and", written "+".
 *                 Compound: the beat divides in three, the first
 *                 division is the beat itself, the second is "la" and
 *                 the third "li": "1 la li 2 la li"
 *   SUBDIVISIONS  simple: the quarter points are "e" (a long vowel, as
 *                 in "see") and "a" ("uh"): "1 e + a". Compound: the
 *                 next level down adds "ta" between the division
 *                 syllables, which stay put: "1 ta la ta li ta"
 *   FINER         simple only: the eighth points, one level further, add
 *                 "ta" between the previous syllables: "1 ta e ta + ta a
 *                 ta"
 *   SILENT        a beat that is not articulated aloud (held by a longer
 *                 note, a rest, or a tie) is written in parentheses:
 *                 "(2)"
 *   DOTS          in simple meter a dotted note also leaves a beat
 *                 uncounted, so it is parenthesized; in compound meter a
 *                 dotted note IS the beat (the beat is dotted), so a dot
 *                 never puts a beat in parentheses
 *
 * Simple meter is counted on a grid of eighths of a beat, compound on a
 * grid of sixths; a position finer than the grid is not counted. The
 * counting syllables are held as a table per meter type so another system
 * can be added alongside later.
 */

typedef enum {
    STANDARD_METER_COUNTING_RULE_BEATS = 0,
    STANDARD_METER_COUNTING_RULE_DIVISIONS,
    STANDARD_METER_COUNTING_RULE_SUBDIVISIONS,
    STANDARD_METER_COUNTING_RULE_FINER,
    STANDARD_METER_COUNTING_RULE_SILENT,
    STANDARD_METER_COUNTING_RULE_DOTS,
    STANDARD_METER_COUNTING_RULE_COUNT
} StandardMeterCountingRule;

/* Positions inside a beat are counted on a grid: eighths of a beat in
   simple meter, sixths in compound. */
#define STANDARD_METER_COUNT_GRID_SIMPLE 8
#define STANDARD_METER_COUNT_GRID_COMPOUND 6

typedef struct {
    unsigned beat;        /* 1-based beat the position falls in */
    unsigned position;    /* grid steps past the beat: 0-7 simple, 0-5 compound */
    unsigned parts;       /* the grid size: 8 simple, 6 compound */
    const char *syllable; /* "+", "e", "a", "ta" (simple); "la", "li", "ta" (compound); NULL on the beat */
} StandardMeterCount;

const Rule *standard_meter_counting_rule(StandardMeterCountingRule which);
const Constraint *standard_meter_counting_constraint(StandardMeterCountingRule which);

/*
 * The count of the position `offset` into a measure (zero is the
 * downbeat), by the signature's meter class. False, leaving *out
 * untouched, when the signature is unclassified, the offset is at or past
 * the end of the measure, or is not a whole number of grid steps of a
 * beat.
 */
bool standard_meter_count(const TimeSignature *time_signature, const Duration *offset, StandardMeterCount *out);

/* DOTS: true when a dot leaves a beat uncounted and so puts it in
   parentheses (simple meter); false for compound meter, where a dotted
   note receives the beat, and for an unclassified type. */
bool standard_meter_dot_silences_beat(MeterType type);

/*
 * The words said for a count: the beat number on the beat ("2"), the
 * syllable off it ("+"). With articulated false the text is in
 * parentheses ("(2)", "(+)"). False when the buffer is too small (size
 * must hold the text and its terminator).
 */
bool standard_meter_count_text(const StandardMeterCount *count, bool articulated, char *buffer, size_t size);

#endif
