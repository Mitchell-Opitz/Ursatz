#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_TIME_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_TIME_H

#include <stdbool.h>

#include "meter_analysis_input.h"
#include "metric_position_rules.h"

/*
 * Internal to the rhythm adapter: exact times, the measure arithmetic over a MeterMap, and the metric reading of a time. A
 * time is a rational of whole notes; nothing here uses floating point for a decision.
 */

typedef struct {
    long long n;
    long long d; /* positive */
} StandardMeterQ;

StandardMeterQ standard_meter_q(long long n, long long d);
int standard_meter_q_compare(StandardMeterQ a, StandardMeterQ b);
StandardMeterQ standard_meter_q_add(StandardMeterQ a, StandardMeterQ b);
StandardMeterQ standard_meter_q_sub(StandardMeterQ a, StandardMeterQ b);
StandardMeterQ standard_meter_q_mul_int(StandardMeterQ a, long long k);
StandardMeterQ standard_meter_q_div_int(StandardMeterQ a, long long k);
double standard_meter_q_double(StandardMeterQ a);

StandardMeterQ standard_meter_q_of_time(const MusicalTime *time);
bool standard_meter_span_q(const TimeSpan *span, StandardMeterQ *start, StandardMeterQ *end);

/* The time snapped per the configuration (an attack, or a release with the wider tolerance); *changed (when not NULL) is set when
   the time moved. */
StandardMeterQ standard_meter_snap(const StandardMeterConfig *config, StandardMeterQ time, bool release, bool *changed);

/* The measure that contains `time` (a barline is the measure that starts there). */
bool standard_meter_measure_at(const MeterMap *map, StandardMeterQ time, int *measure);
/* The [start, end) of measure `index`. */
bool standard_meter_measure_bounds(const MeterMap *map, int index, StandardMeterQ *start, StandardMeterQ *end);
/* The last measure a span ending at `end` touches: the measure of `end`, or the one before when `end` is a barline. */
bool standard_meter_last_measure_touched(const MeterMap *map, StandardMeterQ end, int *measure);

/*
 * The beat grid of measure `index`, as the standard-meter rules read it: the signature positions in the measure are read in
 * (a pickup is read in the signature of the full measure it is the end of), the length of a beat, the beats in a measure,
 * the parts a beat divides into for the off-beat level (2 in simple meter, 3 in compound), the bounds of the measure and
 * `base`, where a full measure with this reading would start (equal to `start` except in a pickup).
 */
typedef struct {
    StandardMeterQ start;
    StandardMeterQ end;
    StandardMeterQ base;
    StandardMeterQ beat;
    unsigned beats;
    unsigned division;
} StandardMeterMeasureGrid;

bool standard_meter_measure_grid(const MeterMap *map, int index, StandardMeterMeasureGrid *grid);

/* The metric level of `time`, as metric_position_rules.h reads it. */
bool standard_meter_level_at(const MeterMap *map, StandardMeterQ time, StandardMeterLevel *level);
/* The measure of `time` and the beat in it, 1-based and fractional (beat 2 and a half is 2.5). */
bool standard_meter_beat_of(const MeterMap *map, StandardMeterQ time, int *measure, double *beat);

#endif
