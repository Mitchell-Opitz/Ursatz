#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_POSITION_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_POSITION_H

#include <stdbool.h>

#include "core/rational.h"
#include "time/meter_map.h"
#include "standard-meter/metric_position_rules.h"

/*
 * Internal to the rhythm adapter: the beat grid of a measure and the metric reading of a time over a MeterMap. A time is a
 * Rational of whole notes; nothing here uses floating point for a decision.
 */

/*
 * The beat grid of measure `index`, as the standard-meter rules read it: the signature positions in the measure are read in
 * (a pickup is read in the signature of the full measure it is the end of), the length of a beat, the beats in a measure,
 * the parts a beat divides into for the off-beat level (2 in simple meter, 3 in compound), the bounds of the measure and
 * `base`, where a full measure with this reading would start (equal to `start` except in a pickup).
 */
typedef struct {
    Rational start;
    Rational end;
    Rational base;
    Rational beat;
    unsigned beats;
    unsigned division;
} StandardMeterMeasureGrid;

bool standard_meter_measure_grid(const MeterMap *map, int index, StandardMeterMeasureGrid *grid);

/* The metric level of `time`, as metric_position_rules.h reads it. */
bool standard_meter_level_at(const MeterMap *map, Rational time, StandardMeterLevel *level);
/* The measure of `time` and the beat in it, 1-based and fractional (beat 2 and a half is 2.5). */
bool standard_meter_beat_of(const MeterMap *map, Rational time, int *measure, double *beat);

#endif
