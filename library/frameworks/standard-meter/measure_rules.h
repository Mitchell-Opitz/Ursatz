#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_MEASURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_MEASURE_RULES_H

#include <stdbool.h>

#include "meter_class_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Measure rules for standard-meter-v1. Bar lines separate music into
 * measures (bars):
 *
 *   MEASURE_GROUP       a measure is exactly one beat group (duple,
 *                       triple, or quadruple) in both simple and compound
 *                       meter, so its length is the top number times the
 *                       bottom unit (6/8 is 3/4, two dotted-quarter
 *                       beats)
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

/* MEASURE_GROUP: the beat grouping one measure of this signature forms,
   read through its meter class (4/4 and 12/8 are both quadruple;
   METER_GROUPING_NONE when unclassified). */
MeterGrouping standard_meter_measure_grouping(const TimeSignature *time_signature);

/* MULTI_MEASURE_REST: the length of a rest of `measures` full measures.
   False, leaving *out untouched, when measures is 0, or on a NULL
   signature or allocation failure. On success the caller owns *out. */
bool standard_meter_multi_measure_rest_duration(const TimeSignature *time_signature, unsigned measures,
                                                Duration **out);

#endif
