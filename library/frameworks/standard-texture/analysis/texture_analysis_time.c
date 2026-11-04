#include "texture_analysis_time.h"

#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/musical_time.h"
#include "time/score_time.h"

static inline Rational to_rational(StandardTextureQ q) {
    return rational_ratio(q.n, q.d);
}

static inline StandardTextureQ of_rational(Rational r) {
    StandardTextureQ q = {r.num, r.den};
    return q;
}

/* A value that does not fit a Rational (overflow, or a zero denominator) reads as 0/1: these constructors and
   operators return a plain value, so 0/1 is their failure value. */
static inline StandardTextureQ zero_q(void) {
    StandardTextureQ q = {0, 1};
    return q;
}

StandardTextureQ standard_texture_q(long long n, long long d) {
    Rational r;
    return rational_make(n, d, &r) == RATIONAL_OK ? of_rational(r) : zero_q();
}

int standard_texture_q_compare(StandardTextureQ a, StandardTextureQ b) {
    return rational_cmp(to_rational(a), to_rational(b));
}

StandardTextureQ standard_texture_q_add(StandardTextureQ a, StandardTextureQ b) {
    Rational sum;
    return rational_add(to_rational(a), to_rational(b), &sum) == RATIONAL_OK ? of_rational(sum) : zero_q();
}

StandardTextureQ standard_texture_q_sub(StandardTextureQ a, StandardTextureQ b) {
    Rational difference;
    return rational_sub(to_rational(a), to_rational(b), &difference) == RATIONAL_OK ? of_rational(difference) : zero_q();
}

double standard_texture_q_double(StandardTextureQ a) {
    return (double)a.n / (double)a.d;
}

bool standard_texture_span_q(const TimeSpan *span, StandardTextureQ *start, StandardTextureQ *end) {
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

/* A time that cannot be snapped without overflow is returned as it is, unchanged. */
StandardTextureQ standard_texture_snap(const StandardTextureConfig *config, StandardTextureQ time, bool release) {
    if (config == NULL || config->onset_grid_denominator == 0 || config->onset_tolerance_denominator == 0 ||
        config->release_tolerance_denominator == 0) {
        return time;
    }
    unsigned tolerance = release ? config->release_tolerance_denominator : config->onset_tolerance_denominator;
    Rational snapped;
    if (!score_time_snap(to_rational(time), config->onset_grid_denominator, tolerance, &snapped, NULL)) {
        return time;
    }
    return of_rational(snapped);
}

bool standard_texture_measure_at(const MeterMap *map, StandardTextureQ time, int *measure) {
    return meter_map_measure_of(map, to_rational(time), measure) == METER_MAP_OK;
}

bool standard_texture_measure_bounds(const MeterMap *map, int index, StandardTextureQ *start, StandardTextureQ *end) {
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

bool standard_texture_last_measure_touched(const MeterMap *map, StandardTextureQ end, int *measure) {
    return meter_map_last_measure_touched(map, to_rational(end), measure) == METER_MAP_OK;
}

bool standard_texture_beat_length(const MeterMap *map, int index, StandardTextureQ *beat) {
    Rational length;
    if (!score_time_beat_length(map, index, &length)) {
        return false;
    }
    *beat = of_rational(length);
    return true;
}

TimeSpan *standard_texture_make_span(StandardTextureQ start, StandardTextureQ end) {
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
