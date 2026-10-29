#include "meter_analysis_time.h"

#include <stdlib.h>

#include "beat_grid.h"
#include "core/rational.h"
#include "meter_class_rules.h"
#include "time/meter_arithmetic.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/time_signature.h"

StandardMeterQ standard_meter_q(long long n, long long d) {
    if (d < 0) {
        n = -n;
        d = -d;
    }
    long long g = rational_gcd(n, d);
    if (g == 0) {
        g = 1;
    }
    StandardMeterQ q = {n / g, d / g};
    return q;
}

static inline Rational to_rational(StandardMeterQ q) {
    return rational_ratio(q.n, q.d);
}

static inline StandardMeterQ of_rational(Rational r) {
    StandardMeterQ q = {r.num, r.den};
    return q;
}

int standard_meter_q_compare(StandardMeterQ a, StandardMeterQ b) {
    return rational_cmp(to_rational(a), to_rational(b));
}

StandardMeterQ standard_meter_q_add(StandardMeterQ a, StandardMeterQ b) {
    Rational sum;
    if (rational_add(to_rational(a), to_rational(b), &sum) == RATIONAL_OK) {
        return of_rational(sum);
    }
    return standard_meter_q(a.n * b.d + b.n * a.d, a.d * b.d);
}

StandardMeterQ standard_meter_q_sub(StandardMeterQ a, StandardMeterQ b) {
    Rational difference;
    if (rational_sub(to_rational(a), to_rational(b), &difference) == RATIONAL_OK) {
        return of_rational(difference);
    }
    return standard_meter_q(a.n * b.d - b.n * a.d, a.d * b.d);
}

StandardMeterQ standard_meter_q_mul_int(StandardMeterQ a, long long k) {
    return standard_meter_q(a.n * k, a.d);
}

StandardMeterQ standard_meter_q_div_int(StandardMeterQ a, long long k) {
    return standard_meter_q(a.n, a.d * k);
}

double standard_meter_q_double(StandardMeterQ a) {
    return (double)a.n / (double)a.d;
}

StandardMeterQ standard_meter_q_of_time(const MusicalTime *time) {
    return standard_meter_q(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

bool standard_meter_span_q(const TimeSpan *span, StandardMeterQ *start, StandardMeterQ *end) {
    if (span == NULL || time_span_start(span) == NULL || time_span_end(span) == NULL) {
        return false;
    }
    if (start != NULL) {
        *start = standard_meter_q_of_time(time_span_start(span));
    }
    if (end != NULL) {
        *end = standard_meter_q_of_time(time_span_end(span));
    }
    return true;
}

StandardMeterQ standard_meter_snap(const StandardMeterConfig *config, StandardMeterQ time, bool release, bool *changed) {
    if (changed != NULL) {
        *changed = false;
    }
    if (config == NULL || config->onset_grid_denominator == 0 || config->onset_tolerance_denominator == 0 ||
        config->release_tolerance_denominator == 0) {
        return time;
    }
    long long grid = (long long)config->onset_grid_denominator;
    /* nearest multiple of 1/grid: round(time * grid) / grid, with the floor taken toward minus infinity */
    long long scaled_n = time.n * grid;
    long long whole = scaled_n / time.d;
    long long rest = scaled_n % time.d;
    if (rest < 0) {
        whole -= 1;
        rest += time.d;
    }
    if (2 * rest >= time.d) {
        whole += 1;
    }
    StandardMeterQ nearest = standard_meter_q(whole, grid);
    StandardMeterQ gap = standard_meter_q_sub(time, nearest);
    if (gap.n < 0) {
        gap.n = -gap.n;
    }
    StandardMeterQ tolerance = standard_meter_q(
        1, (long long)(release ? config->release_tolerance_denominator : config->onset_tolerance_denominator));
    if (standard_meter_q_compare(gap, tolerance) <= 0) {
        if (changed != NULL) {
            *changed = standard_meter_q_compare(time, nearest) != 0;
        }
        return nearest;
    }
    return time;
}

static bool time_of(StandardMeterQ q, MusicalTime **out) {
    return q.n >= 0 && musical_time_create((long)q.n, (unsigned long)q.d, out) == MUSICAL_TIME_OK;
}

bool standard_meter_measure_at(const MeterMap *map, StandardMeterQ time, int *measure) {
    return meter_map_measure_of(map, to_rational(time), measure) == METER_MAP_OK;
}

bool standard_meter_measure_bounds(const MeterMap *map, int index, StandardMeterQ *start, StandardMeterQ *end) {
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

bool standard_meter_last_measure_touched(const MeterMap *map, StandardMeterQ end, int *measure) {
    return meter_map_last_measure_touched(map, to_rational(end), measure) == METER_MAP_OK;
}

static StandardMeterQ q_of_duration(const Duration *duration) {
    return standard_meter_q(duration_numerator(duration), (long long)duration_denominator(duration));
}

bool standard_meter_measure_grid(const MeterMap *map, int index, StandardMeterMeasureGrid *grid) {
    if (grid == NULL || !standard_meter_measure_bounds(map, index, &grid->start, &grid->end)) {
        return false;
    }
    const TimeSignature *signature = NULL;
    if (metrical_position_reading_signature(map, index, &signature) != METRICAL_POSITION_OK) {
        return false;
    }
    Duration *beat = NULL;
    unsigned beats = 0;
    if (!standard_meter_beat_grid(signature, &beat, &beats)) {
        duration_destroy(beat);
        return false;
    }
    grid->beat = q_of_duration(beat);
    grid->beats = beats;
    duration_destroy(beat);
    grid->division = standard_meter_type(signature) == METER_TYPE_COMPOUND ? 3 : 2;

    MusicalTime *at = NULL;
    MetricalPosition *position = NULL;
    if (!time_of(grid->start, &at)) {
        return false;
    }
    bool ok = metrical_position_from_time(map, at, &position) == METRICAL_POSITION_OK;
    musical_time_destroy(at);
    if (!ok) {
        return false;
    }
    grid->base = standard_meter_q_sub(grid->start, q_of_duration(metrical_position_offset(position)));
    metrical_position_destroy(position);
    return true;
}

static bool position_at(const MeterMap *map, StandardMeterQ time, MetricalPosition **position, const TimeSignature **signature) {
    MusicalTime *at = NULL;
    if (map == NULL || !time_of(time, &at)) {
        return false;
    }
    bool ok = metrical_position_from_time(map, at, position) == METRICAL_POSITION_OK;
    musical_time_destroy(at);
    if (!ok) {
        return false;
    }
    if (metrical_position_reading_signature(map, metrical_position_measure(*position), signature) != METRICAL_POSITION_OK) {
        metrical_position_destroy(*position);
        *position = NULL;
        return false;
    }
    return true;
}

bool standard_meter_level_at(const MeterMap *map, StandardMeterQ time, StandardMeterLevel *level) {
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    if (level == NULL || !position_at(map, time, &position, &signature)) {
        return false;
    }
    bool ok = standard_meter_metric_level(signature, position, level);
    metrical_position_destroy(position);
    return ok;
}

bool standard_meter_beat_of(const MeterMap *map, StandardMeterQ time, int *measure, double *beat) {
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    if (measure == NULL || beat == NULL || !position_at(map, time, &position, &signature)) {
        return false;
    }
    Duration *unit = NULL;
    unsigned beats = 0;
    bool ok = standard_meter_beat_grid(signature, &unit, &beats);
    if (ok) {
        StandardMeterQ length = q_of_duration(unit);
        StandardMeterQ offset = q_of_duration(metrical_position_offset(position));
        *measure = metrical_position_measure(position);
        *beat = 1.0 + standard_meter_q_double(offset) / standard_meter_q_double(length);
    }
    duration_destroy(unit);
    metrical_position_destroy(position);
    return ok;
}
