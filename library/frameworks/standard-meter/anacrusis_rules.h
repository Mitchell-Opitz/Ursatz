#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANACRUSIS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANACRUSIS_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"

/*
 * Anacrusis (pickup) rules for standard-meter-v1. A work that begins
 * with a pickup note or notes:
 *
 *   PICKUP_VALID       a pickup is shorter than a full measure (and not
 *                      empty); a pickup as long as a measure is just a
 *                      measure
 *   PICKUP_POSITION    the pickup counts as the last notes of an
 *                      imaginary measure, so counting does not begin on
 *                      1: it starts at the measure length minus the
 *                      pickup length
 *   FINAL_MEASURE      the work's last measure is usually shortened by
 *                      the pickup's length
 *
 * All lengths are exact Durations.
 */

typedef enum {
    STANDARD_METER_ANACRUSIS_RULE_PICKUP_VALID = 0,
    STANDARD_METER_ANACRUSIS_RULE_PICKUP_POSITION,
    STANDARD_METER_ANACRUSIS_RULE_FINAL_MEASURE,
    STANDARD_METER_ANACRUSIS_RULE_COUNT
} StandardMeterAnacrusisRule;

const Rule *standard_meter_anacrusis_rule(StandardMeterAnacrusisRule which);
const Constraint *standard_meter_anacrusis_constraint(StandardMeterAnacrusisRule which);

/* PICKUP_VALID: true when 0 < pickup < one measure of this signature. */
bool standard_meter_pickup_valid(const TimeSignature *time_signature, const Duration *pickup);

/*
 * PICKUP_POSITION: where the pickup starts inside its imaginary measure,
 * measured from that measure's start (measure length minus pickup
 * length). For one quarter-note pickup in 4/4 this is 3/4: the pickup is
 * counted as beat 4. False, leaving *out untouched, when the pickup is not
 * valid or on allocation failure. On success the caller owns *out.
 */
bool standard_meter_pickup_start_offset(const TimeSignature *time_signature, const Duration *pickup,
                                        Duration **out);

/*
 * FINAL_MEASURE: the usual length of the work's last measure when it
 * begins with this pickup (measure length minus pickup length). False,
 * leaving *out untouched, when the pickup is not valid or on allocation
 * failure. On success the caller owns *out.
 */
bool standard_meter_final_measure_duration(const TimeSignature *time_signature, const Duration *pickup,
                                           Duration **out);

#endif
