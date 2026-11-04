#ifndef MUSIC_TIME_SCORE_TIME_H
#define MUSIC_TIME_SCORE_TIME_H

#include <stdbool.h>

#include "core/rational.h"
#include "time/meter_map.h"
#include "time/time_span.h"

/*
 * Score time: the one kernel owner of the exact time reads an analysis adapter needs over a MeterMap. A time is a
 * Rational of whole notes (core/rational.h) and every step is checked. Each function returns false when its
 * arguments are invalid, the map has no such measure, or the arithmetic does not fit a Rational; outputs are written
 * only on true. There is no unchecked fallback: an overflow is a refusal the caller handles.
 *
 * The framework time modules (standard-meter, standard-form, standard-texture) are thin wrappers over these four
 * functions until they are retired.
 */

/* The [start, end) of a span, read exactly. Either output may be NULL. */
bool score_time_span(const TimeSpan *span, Rational *start, Rational *end);

/* The [start, end) of measure `index` in `map`. Either output may be NULL. */
bool score_time_measure_bounds(const MeterMap *map, int index, Rational *start, Rational *end);

/* The length of one beat in measure `index`: the unit of its time signature, or three units in a compound meter. */
bool score_time_beat_length(const MeterMap *map, int index, Rational *beat);

/*
 * `time` snapped to the nearest multiple of 1/grid_denominator when it lies within 1/tolerance_denominator of it
 * (a half rounds up, and a negative time rounds toward minus infinity first); otherwise `time` itself. *changed
 * (when not NULL) is set when the time moved. A zero grid or tolerance denominator leaves the time unchanged and
 * is not an error.
 */
bool score_time_snap(Rational time, unsigned grid_denominator, unsigned tolerance_denominator, Rational *snapped,
                     bool *changed);

#endif
