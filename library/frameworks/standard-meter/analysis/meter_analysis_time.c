#include "meter_analysis_time.h"

#include <stdlib.h>

#include "beat_grid.h"
#include "core/rational.h"
#include "meter_class_rules.h"
#include "time/meter_arithmetic.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/score_time.h"
#include "time/time_rational.h"
#include "time/time_signature.h"

/* A value that does not fit a Rational (overflow, or a zero denominator) reads as 0/1: these constructors and
   operators return a plain value, so 0/1 is their failure value. Callers that must see a refusal use the checked
   score_time_* functions. */
static inline StandardMeterQ zero_q(void) {
    StandardMeterQ q = {0, 1};
    return q;
}

static inline Rational to_rational(StandardMeterQ q) {
    return rational_ratio(q.n, q.d);
}

static inline StandardMeterQ of_rational(Rational r) {
    StandardMeterQ q = {r.num, r.den};
    return q;
}

StandardMeterQ standard_meter_q(long long n, long long d) {
    Rational r;
    return rational_make(n, d, &r) == RATIONAL_OK ? of_rational(r) : zero_q();
}

int standard_meter_q_compare(StandardMeterQ a, StandardMeterQ b) {
    return rational_cmp(to_rational(a), to_rational(b));
}

StandardMeterQ standard_meter_q_add(StandardMeterQ a, StandardMeterQ b) {
    Rational sum;
    return rational_add(to_rational(a), to_rational(b), &sum) == RATIONAL_OK ? of_rational(sum) : zero_q();
}

StandardMeterQ standard_meter_q_sub(StandardMeterQ a, StandardMeterQ b) {
    Rational difference;
    return rational_sub(to_rational(a), to_rational(b), &difference) == RATIONAL_OK ? of_rational(difference) : zero_q();
}

StandardMeterQ standard_meter_q_mul_int(StandardMeterQ a, long long k) {
    Rational product;
    return rational_mul_int(to_rational(a), k, &product) == RATIONAL_OK ? of_rational(product) : zero_q();
}

StandardMeterQ standard_meter_q_div_int(StandardMeterQ a, long long k) {
    Rational quotient;
    return rational_div_int(to_rational(a), k, &quotient) == RATIONAL_OK ? of_rational(quotient) : zero_q();
}

double standard_meter_q_double(StandardMeterQ a) {
    return (double)a.n / (double)a.d;
}

StandardMeterQ standard_meter_q_of_time(const MusicalTime *time) {
    Rational r;
    return time != NULL && rational_of_musical_time(time, &r) == RATIONAL_OK ? of_rational(r) : zero_q();
}

bool standard_meter_span_q(const TimeSpan *span, StandardMeterQ *start, StandardMeterQ *end) {
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
StandardMeterQ standard_meter_snap(const StandardMeterConfig *config, StandardMeterQ time, bool release, bool *changed) {
    if (changed != NULL) {
        *changed = false;
    }
    if (config == NULL || config->onset_grid_denominator == 0 || config->onset_tolerance_denominator == 0 ||
        config->release_tolerance_denominator == 0) {
        return time;
    }
    unsigned tolerance = release ? config->release_tolerance_denominator : config->onset_tolerance_denominator;
    Rational snapped;
    bool moved = false;
    if (!score_time_snap(to_rational(time), config->onset_grid_denominator, tolerance, &snapped, &moved)) {
        return time;
    }
    if (changed != NULL) {
        *changed = moved;
    }
    return of_rational(snapped);
}

static bool time_of(Rational q, MusicalTime **out) {
    return q.num >= 0 && musical_time_create((long)q.num, (unsigned long)q.den, out) == MUSICAL_TIME_OK;
}

bool standard_meter_measure_at(const MeterMap *map, StandardMeterQ time, int *measure) {
    return meter_map_measure_of(map, to_rational(time), measure) == METER_MAP_OK;
}

bool standard_meter_measure_bounds(const MeterMap *map, int index, StandardMeterQ *start, StandardMeterQ *end) {
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

bool standard_meter_last_measure_touched(const MeterMap *map, StandardMeterQ end, int *measure) {
    return meter_map_last_measure_touched(map, to_rational(end), measure) == METER_MAP_OK;
}

static bool rational_of_length(const Duration *duration, Rational *out) {
    return rational_make(duration_numerator(duration), (long long)duration_denominator(duration), out) == RATIONAL_OK;
}

bool standard_meter_measure_grid(const MeterMap *map, int index, StandardMeterMeasureGrid *grid) {
    if (grid == NULL || !score_time_measure_bounds(map, index, &grid->start, &grid->end)) {
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
    bool beat_ok = rational_of_length(beat, &grid->beat);
    duration_destroy(beat);
    if (!beat_ok) {
        return false;
    }
    grid->beats = beats;
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
    Rational offset;
    ok = rational_of_length(metrical_position_offset(position), &offset) &&
         rational_sub(grid->start, offset, &grid->base) == RATIONAL_OK;
    metrical_position_destroy(position);
    return ok;
}

static bool position_at(const MeterMap *map, Rational time, MetricalPosition **position, const TimeSignature **signature) {
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

bool standard_meter_level_at(const MeterMap *map, Rational time, StandardMeterLevel *level) {
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    if (level == NULL || !position_at(map, time, &position, &signature)) {
        return false;
    }
    bool ok = standard_meter_metric_level(signature, position, level);
    metrical_position_destroy(position);
    return ok;
}

bool standard_meter_beat_of(const MeterMap *map, Rational time, int *measure, double *beat) {
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    if (measure == NULL || beat == NULL || !position_at(map, time, &position, &signature)) {
        return false;
    }
    Duration *unit = NULL;
    unsigned beats = 0;
    bool ok = standard_meter_beat_grid(signature, &unit, &beats);
    Rational length;
    Rational offset;
    ok = ok && rational_of_length(unit, &length) && rational_of_length(metrical_position_offset(position), &offset);
    if (ok) {
        *measure = metrical_position_measure(position);
        *beat = 1.0 + rational_to_double(offset) / rational_to_double(length);
    }
    duration_destroy(unit);
    metrical_position_destroy(position);
    return ok;
}
