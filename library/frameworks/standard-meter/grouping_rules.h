#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_GROUPING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_GROUPING_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Beat and beat-grouping rules for standard-meter-v1. A beat is a pulse
 * in music that regularly recurs; beats group into measures:
 *
 *   BEAT_DIVISION  in a simple meter the beat divides into two, and each
 *                  half further subdivides, giving four per beat
 *   GROUPING       two beats per group is duple, three triple, four
 *                  quadruple
 *   BEAT_ROLES     beat 1 of a group is the downbeat and carries more
 *                  "weight"; the last beat is the upbeat and is
 *                  anticipatory
 *
 * That a simple quadruple meter can feel like two groups of two (and so
 * be hard to tell from duple by ear) is a perceptual ambiguity, not a
 * notation rule, and is not asserted here.
 */

typedef enum {
    STANDARD_METER_GROUPING_RULE_BEAT_DIVISION = 0,
    STANDARD_METER_GROUPING_RULE_GROUPING,
    STANDARD_METER_GROUPING_RULE_BEAT_ROLES,
    STANDARD_METER_GROUPING_RULE_COUNT
} StandardMeterGroupingRule;

typedef enum {
    METER_GROUPING_NONE,
    METER_GROUPING_DUPLE,
    METER_GROUPING_TRIPLE,
    METER_GROUPING_QUADRUPLE
} MeterGrouping;

typedef enum {
    BEAT_ROLE_DOWNBEAT, /* beat 1 */
    BEAT_ROLE_MIDDLE,
    BEAT_ROLE_UPBEAT /* the last beat */
} BeatRole;

/* Deepest division level BEAT_DIVISION answers for (2^6 parts). */
#define STANDARD_METER_MAX_DIVISION_DEPTH 6

const Rule *standard_meter_grouping_rule(StandardMeterGroupingRule which);
const Constraint *standard_meter_grouping_constraint(StandardMeterGroupingRule which);

/*
 * BEAT_DIVISION: how many parts one beat is split into at the given
 * depth in a simple meter: depth 0 is the beat itself (1), depth 1 the
 * division (2), depth 2 the subdivision (4), and so on (2^depth). False,
 * leaving *out_parts untouched, past STANDARD_METER_MAX_DIVISION_DEPTH.
 */
bool standard_meter_beat_division_parts(unsigned depth, unsigned *out_parts);

/* GROUPING: duple for 2 beats, triple for 3, quadruple for 4;
   METER_GROUPING_NONE for any other count. */
MeterGrouping standard_meter_grouping(unsigned beats);

/*
 * BEAT_ROLES: the role of beat `beat` (counted from 1) in a group of
 * `beats` beats. A one-beat group's only beat is its downbeat. False,
 * leaving *out untouched, when beat is 0 or past beats.
 */
bool standard_meter_beat_role(unsigned beat, unsigned beats, BeatRole *out);

#endif
