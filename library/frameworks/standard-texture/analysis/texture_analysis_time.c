#include "texture_analysis_time.h"

#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/musical_time.h"

StandardTextureQ standard_texture_q(long long n, long long d) {
    if (d < 0) {
        n = -n;
        d = -d;
    }
    long long g = rational_gcd(n, d);
    if (g == 0) {
        g = 1;
    }
    StandardTextureQ q = {n / g, d / g};
    return q;
}

static inline Rational to_rational(StandardTextureQ q) {
    return rational_ratio(q.n, q.d);
}

static inline StandardTextureQ of_rational(Rational r) {
    StandardTextureQ q = {r.num, r.den};
    return q;
}

int standard_texture_q_compare(StandardTextureQ a, StandardTextureQ b) {
    return rational_cmp(to_rational(a), to_rational(b));
}

StandardTextureQ standard_texture_q_add(StandardTextureQ a, StandardTextureQ b) {
    Rational sum;
    if (rational_add(to_rational(a), to_rational(b), &sum) == RATIONAL_OK) {
        return of_rational(sum);
    }
    return standard_texture_q(a.n * b.d + b.n * a.d, a.d * b.d);
}

StandardTextureQ standard_texture_q_sub(StandardTextureQ a, StandardTextureQ b) {
    Rational difference;
    if (rational_sub(to_rational(a), to_rational(b), &difference) == RATIONAL_OK) {
        return of_rational(difference);
    }
    return standard_texture_q(a.n * b.d - b.n * a.d, a.d * b.d);
}

double standard_texture_q_double(StandardTextureQ a) {
    return (double)a.n / (double)a.d;
}

static StandardTextureQ of_time(const MusicalTime *time) {
    return standard_texture_q(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

bool standard_texture_span_q(const TimeSpan *span, StandardTextureQ *start, StandardTextureQ *end) {
    if (span == NULL || time_span_start(span) == NULL || time_span_end(span) == NULL) {
        return false;
    }
    if (start != NULL) {
        *start = of_time(time_span_start(span));
    }
    if (end != NULL) {
        *end = of_time(time_span_end(span));
    }
    return true;
}

StandardTextureQ standard_texture_snap(const StandardTextureConfig *config, StandardTextureQ time, bool release) {
    if (config == NULL || config->onset_grid_denominator == 0 || config->onset_tolerance_denominator == 0 ||
        config->release_tolerance_denominator == 0) {
        return time;
    }
    long long grid = (long long)config->onset_grid_denominator;
    long long scaled = time.n * grid;
    long long whole = scaled / time.d;
    long long rest = scaled % time.d;
    if (rest < 0) {
        whole -= 1;
        rest += time.d;
    }
    if (2 * rest >= time.d) {
        whole += 1;
    }
    StandardTextureQ nearest = standard_texture_q(whole, grid);
    StandardTextureQ gap = standard_texture_q_sub(time, nearest);
    if (gap.n < 0) {
        gap.n = -gap.n;
    }
    StandardTextureQ tolerance =
        standard_texture_q(1, (long long)(release ? config->release_tolerance_denominator : config->onset_tolerance_denominator));
    return standard_texture_q_compare(gap, tolerance) <= 0 ? nearest : time;
}

bool standard_texture_measure_at(const MeterMap *map, StandardTextureQ time, int *measure) {
    return meter_map_measure_of(map, to_rational(time), measure) == METER_MAP_OK;
}

bool standard_texture_measure_bounds(const MeterMap *map, int index, StandardTextureQ *start, StandardTextureQ *end) {
    Rational from;
    Rational to;
    if (meter_map_measure_bounds(map, index, &from, &to) != METER_MAP_OK) {
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
    if (beat == NULL || meter_map_beat_length(map, index, &length) != METER_MAP_OK) {
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
