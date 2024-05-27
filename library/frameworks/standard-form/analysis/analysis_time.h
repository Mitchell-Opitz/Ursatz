#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_TIME_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_TIME_H

#include <stdbool.h>

#include "analysis_input.h"

/*
 * Internal to the form adapter: exact times and the measure arithmetic over a MeterMap. A time is a rational of whole notes;
 * nothing here uses floating point.
 */

typedef struct {
    long long n;
    long long d; /* positive */
} StandardFormQ;

StandardFormQ standard_form_q(long long n, long long d);
int standard_form_q_compare(StandardFormQ a, StandardFormQ b);
StandardFormQ standard_form_q_add(StandardFormQ a, StandardFormQ b);
StandardFormQ standard_form_q_sub(StandardFormQ a, StandardFormQ b);

bool standard_form_span_q(const TimeSpan *span, StandardFormQ *start, StandardFormQ *end);

/* The measure that contains `time` (a barline is the measure that starts there). */
bool standard_form_measure_at(const MeterMap *map, StandardFormQ time, int *measure);
/* The last measure a span ending at `end` touches: the measure of `end`, or the one before when `end` is a barline. */
bool standard_form_last_measure_touched(const MeterMap *map, StandardFormQ end, int *measure);
/* The [start, end) of measure `index`. */
bool standard_form_measure_bounds(const MeterMap *map, int index, StandardFormQ *start, StandardFormQ *end);
/* The beat length at measure `index`: the unit of its time signature, or three units in a compound meter (a top number of 6, 9
   or 12 over an eighth or smaller unit). */
bool standard_form_beat_length(const MeterMap *map, int index, StandardFormQ *beat);
/* The first beat of measure `index` at or after `time`, never later than the measure's end. */
bool standard_form_snap_to_beat(const MeterMap *map, int index, StandardFormQ time, StandardFormQ *snapped);

/* True when `time` falls on a beat of its measure (the measure's start included). */
bool standard_form_on_beat(const MeterMap *map, StandardFormQ time);

/* A new left-closed TimeSpan [start, end) the caller destroys, or NULL. */
TimeSpan *standard_form_make_span(StandardFormQ start, StandardFormQ end);

/* The first measure that counts toward lengths: 1 when the map has a pickup (measure 0 is not counted), else the first measure. */
int standard_form_first_counted_measure(const MeterMap *map);

#endif
