#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_MEASURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_MEASURE_RULES_H

#include <stdbool.h>

#include "grouping_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Measure rules for standard-meter-v1. Bar lines separate music into
 * measures (bars):
 *
 *   MEASURE_GROUP       a measure is exactly one beat group (duple,
 *                       triple, or quadruple), so its length is the beat
 *                       count times the beat unit
 *   MULTI_MEASURE_REST  a rest of several full measures is written as one
 *                       symbol with the number of measures; it lasts that
 *                       many times the measure length
 *
 * The Measure container (containers/measure.h) is untouched; these rules
 * take a TimeSignature and work in plain Durations.
 */

typedef enum {
    STANDARD_METER_MEASURE_RULE_GROUP = 0,
    STANDARD_METER_MEASURE_RULE_MULTI_MEASURE_REST,
    STANDARD_METER_MEASURE_RULE_COUNT
} StandardMeterMeasureRule;

const Rule *standard_meter_measure_rule(StandardMeterMeasureRule which);
const Constraint *standard_meter_measure_constraint(StandardMeterMeasureRule which);

/* MEASURE_GROUP: the exact length of one measure. False, leaving *out
   untouched, on a NULL signature or allocation failure. On success the
   caller owns *out. */
bool standard_meter_measure_duration(const TimeSignature *time_signature, Duration **out);

/* MEASURE_GROUP: the beat grouping one measure of this signature forms
   (METER_GROUPING_NONE for a beat count other than 2, 3, or 4). */
MeterGrouping standard_meter_measure_grouping(const TimeSignature *time_signature);

/* MULTI_MEASURE_REST: the length of a rest of `measures` full measures.
   False, leaving *out untouched, when measures is 0, or on a NULL
   signature or allocation failure. On success the caller owns *out. */
bool standard_meter_multi_measure_rest_duration(const TimeSignature *time_signature, unsigned measures,
                                                Duration **out);

#endif
