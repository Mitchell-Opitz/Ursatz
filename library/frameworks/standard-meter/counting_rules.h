#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_COUNTING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_COUNTING_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Counting rules for standard-meter-v1: American traditional counting,
 * the system Open Music Theory uses (others exist). A position in a
 * measure is counted relative to its beat, whatever the beat unit, so the
 * same rhythm in 4/4, 4/2, 4/8, and 4/16 is counted the same way:
 *
 *   BEATS         each beat is its Arabic numeral: "1 2 3 4"
 *   DIVISIONS     halfway through a beat is "and", written "+"
 *   SUBDIVISIONS  the quarter points are "e" (a long vowel, as in "see")
 *                 and "a" ("uh"): "1 e + a"
 *   FINER         the eighth points, one level further, add "ta" between
 *                 the previous syllables: "1 ta e ta + ta a ta"
 *   SILENT        a beat that is not articulated aloud (held by a longer
 *                 note, a rest, a tie, or a dot) is written in
 *                 parentheses: "(2)"
 *
 * Counting reaches eighths of a beat; a position finer than that is not
 * counted. The counting syllables are held as a table so another system
 * can be added alongside later.
 */

typedef enum {
    STANDARD_METER_COUNTING_RULE_BEATS = 0,
    STANDARD_METER_COUNTING_RULE_DIVISIONS,
    STANDARD_METER_COUNTING_RULE_SUBDIVISIONS,
    STANDARD_METER_COUNTING_RULE_FINER,
    STANDARD_METER_COUNTING_RULE_SILENT,
    STANDARD_METER_COUNTING_RULE_COUNT
} StandardMeterCountingRule;

/* Positions inside a beat are counted on a grid of eighths of a beat. */
#define STANDARD_METER_COUNT_GRID 8

typedef struct {
    unsigned beat;        /* 1-based beat the position falls in */
    unsigned eighths;     /* eighths of a beat past the beat, 0 through 7 */
    const char *syllable; /* "+", "e", "a", or "ta"; NULL on the beat itself */
} StandardMeterCount;

const Rule *standard_meter_counting_rule(StandardMeterCountingRule which);
const Constraint *standard_meter_counting_constraint(StandardMeterCountingRule which);

/*
 * The count of the position `offset` into a measure (zero is the
 * downbeat). False, leaving *out untouched, when the offset is at or past
 * the end of the measure, or is not a whole number of eighths of a beat.
 */
bool standard_meter_count(const TimeSignature *time_signature, const Duration *offset, StandardMeterCount *out);

/*
 * The words said for a count: the beat number on the beat ("2"), the
 * syllable off it ("+"). With articulated false the text is in
 * parentheses ("(2)", "(+)"). False when the buffer is too small (size
 * must hold the text and its terminator).
 */
bool standard_meter_count_text(const StandardMeterCount *count, bool articulated, char *buffer, size_t size);

#endif
