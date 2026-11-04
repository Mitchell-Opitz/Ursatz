#include "analysis_time.h"

#include <stdlib.h>

#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/musical_time.h"
#include "time/score_time.h"

static inline Rational to_rational(StandardFormQ q) {
    return rational_ratio(q.n, q.d);
}

static inline StandardFormQ of_rational(Rational r) {
    StandardFormQ q = {r.num, r.den};
    return q;
}

/* A value that does not fit a Rational (overflow, or a zero denominator) reads as 0/1: these constructors and
   operators return a plain value, so 0/1 is their failure value. */
static inline StandardFormQ zero_q(void) {
    StandardFormQ q = {0, 1};
    return q;
}

StandardFormQ standard_form_q(long long n, long long d) {
    Rational r;
    return rational_make(n, d, &r) == RATIONAL_OK ? of_rational(r) : zero_q();
}

int standard_form_q_compare(StandardFormQ a, StandardFormQ b) {
    return rational_cmp(to_rational(a), to_rational(b));
}

StandardFormQ standard_form_q_add(StandardFormQ a, StandardFormQ b) {
    Rational sum;
    return rational_add(to_rational(a), to_rational(b), &sum) == RATIONAL_OK ? of_rational(sum) : zero_q();
}

StandardFormQ standard_form_q_sub(StandardFormQ a, StandardFormQ b) {
    Rational difference;
    return rational_sub(to_rational(a), to_rational(b), &difference) == RATIONAL_OK ? of_rational(difference) : zero_q();
}

bool standard_form_span_q(const TimeSpan *span, StandardFormQ *start, StandardFormQ *end) {
    Rational from;
    Rational to;
    if (!score_time_span(span, &from, &to)) {
        return false;
    }
    if (start != NULL) {
        *start = of_rational(from);
    }
    if (end != NULL) {
        *end = of_rational(to);
    }
    return true;
}

bool standard_form_measure_at(const MeterMap *map, StandardFormQ time, int *measure) {
    return meter_map_measure_of(map, to_rational(time), measure) == METER_MAP_OK;
}

bool standard_form_measure_bounds(const MeterMap *map, int index, StandardFormQ *start, StandardFormQ *end) {
    Rational from;
    Rational to;
    if (!score_time_measure_bounds(map, index, &from, &to)) {
        return false;
    }
    if (start != NULL) {
        *start = of_rational(from);
    }
    if (end != NULL) {
        *end = of_rational(to);
    }
    return true;
}

bool standard_form_last_measure_touched(const MeterMap *map, StandardFormQ end, int *measure) {
    return meter_map_last_measure_touched(map, to_rational(end), measure) == METER_MAP_OK;
}

bool standard_form_beat_length(const MeterMap *map, int index, StandardFormQ *beat) {
    Rational length;
    if (!score_time_beat_length(map, index, &length)) {
        return false;
    }
    *beat = of_rational(length);
    return true;
}

bool standard_form_snap_to_beat(const MeterMap *map, int index, StandardFormQ time, StandardFormQ *snapped) {
    Rational result;
    if (snapped == NULL || meter_map_snap_to_beat(map, index, to_rational(time), &result) != METER_MAP_OK) {
        return false;
    }
    *snapped = of_rational(result);
    return true;
}

int standard_form_first_counted_measure(const MeterMap *map) {
    return meter_map_first_counted_measure(map);
}

TimeSpan *standard_form_make_span(StandardFormQ start, StandardFormQ end) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    if (start.n >= 0 && end.n >= 0 && musical_time_create((long)start.n, (unsigned long)start.d, &from) == MUSICAL_TIME_OK &&
        musical_time_create((long)end.n, (unsigned long)end.d, &to) == MUSICAL_TIME_OK) {
        if (time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
            span = NULL;
        }
    }
    musical_time_destroy(from);
    musical_time_destroy(to);
    return span;
}

bool standard_form_on_beat(const MeterMap *map, StandardFormQ time) {
    return meter_map_on_beat(map, to_rational(time));
}
