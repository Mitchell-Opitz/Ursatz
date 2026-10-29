#include "time/meter_arithmetic.h"

#include <limits.h>

#include "time/duration.h"
#include "time/musical_time.h"
#include "time/time_rational.h"
#include "time/time_signature.h"

static MeterMapResult from_rational(RationalResult result) {
    switch (result) {
    case RATIONAL_OK:
        return METER_MAP_OK;
    case RATIONAL_ERR_NULL:
        return METER_MAP_ERR_NULL_ARGUMENT;
    default:
        return METER_MAP_ERR_OVERFLOW;
    }
}

#define TRY(call)                                  \
    do {                                           \
        MeterMapResult try_result_ = (call);       \
        if (try_result_ != METER_MAP_OK) {         \
            return try_result_;                    \
        }                                          \
    } while (0)

static MeterMapResult measure_index_at(const MeterMap *map, Rational time, int *measure) {
    if (rational_is_negative(time)) {
        return METER_MAP_ERR_TIME_BEFORE_START;
    }
    MusicalTime *at = NULL;
    TimeRationalResult built = musical_time_from_rational(time, &at);
    if (built != TIME_RATIONAL_OK) {
        return built == TIME_RATIONAL_ERR_ALLOC ? METER_MAP_ERR_ALLOC : METER_MAP_ERR_OVERFLOW;
    }
    MeterMapResult result = meter_map_measure_index_at(map, at, measure);
    musical_time_destroy(at);
    return result;
}

MeterMapResult meter_map_measure_of(const MeterMap *map, Rational time, int *measure) {
    if (map == NULL || measure == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    int index = 0;
    TRY(measure_index_at(map, time, &index));
    *measure = index;
    return METER_MAP_OK;
}

MeterMapResult meter_map_measure_bounds(const MeterMap *map, int measure, Rational *start, Rational *end) {
    if (map == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    TimeSpan *span = NULL;
    TRY(meter_map_measure_span(map, measure, &span));
    Rational from;
    Rational to;
    MeterMapResult result = from_rational(rational_of_musical_time(time_span_start(span), &from));
    if (result == METER_MAP_OK) {
        result = from_rational(rational_of_musical_time(time_span_end(span), &to));
    }
    time_span_destroy(span);
    if (result == METER_MAP_OK) {
        if (start != NULL) {
            *start = from;
        }
        if (end != NULL) {
            *end = to;
        }
    }
    return result;
}

MeterMapResult meter_map_last_measure_touched(const MeterMap *map, Rational end, int *measure) {
    if (map == NULL || measure == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    int index = 0;
    Rational start;
    TRY(measure_index_at(map, end, &index));
    TRY(meter_map_measure_bounds(map, index, &start, NULL));
    if (rational_cmp(end, start) == 0 && index > meter_map_first_measure_number(map)) {
        index--;
    }
    *measure = index;
    return METER_MAP_OK;
}

MeterMapResult meter_map_beat_length(const MeterMap *map, int measure, Rational *beat) {
    if (map == NULL || beat == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    const MeterMapRegion *region = NULL;
    TRY(meter_map_region_of_measure(map, measure, &region));
    const TimeSignature *signature = meter_map_region_time_signature(region);
    Duration *unit = NULL;
    if (signature == NULL || time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return METER_MAP_ERR_NOT_FOUND;
    }
    Rational length;
    RationalResult read = rational_of_duration(unit, &length);
    duration_destroy(unit);
    TRY(from_rational(read));
    unsigned top = time_signature_top_number(signature);
    if ((top == 6 || top == 9 || top == 12) && time_signature_bottom_number(signature) >= 8) {
        TRY(from_rational(rational_mul_int(length, 3, &length)));
    }
    *beat = length;
    return METER_MAP_OK;
}

MeterMapResult meter_map_beat_count(const MeterMap *map, int measure, int *count) {
    if (count == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    Rational start;
    Rational end;
    Rational beat;
    Rational span;
    Rational beats;
    long long whole = 0;
    TRY(meter_map_measure_bounds(map, measure, &start, &end));
    TRY(meter_map_beat_length(map, measure, &beat));
    TRY(from_rational(rational_sub(end, start, &span)));
    TRY(from_rational(rational_div(span, beat, &beats)));
    TRY(from_rational(rational_ceil(beats, &whole)));
    if (whole > INT_MAX) {
        return METER_MAP_ERR_OVERFLOW;
    }
    *count = (int)whole;
    return METER_MAP_OK;
}

MeterMapResult meter_map_beat_of(const MeterMap *map, Rational time, int *measure, int *beat, Rational *offset) {
    if (map == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    int index = 0;
    Rational start;
    Rational length;
    Rational delta;
    Rational steps;
    Rational whole_beats;
    Rational rest;
    long long number = 0;
    TRY(measure_index_at(map, time, &index));
    TRY(meter_map_measure_bounds(map, index, &start, NULL));
    TRY(meter_map_beat_length(map, index, &length));
    TRY(from_rational(rational_sub(time, start, &delta)));
    TRY(from_rational(rational_div(delta, length, &steps)));
    TRY(from_rational(rational_floor(steps, &number)));
    if (number >= INT_MAX) {
        return METER_MAP_ERR_OVERFLOW;
    }
    TRY(from_rational(rational_mul_int(length, number, &whole_beats)));
    TRY(from_rational(rational_sub(delta, whole_beats, &rest)));
    if (measure != NULL) {
        *measure = index;
    }
    if (beat != NULL) {
        *beat = (int)number + 1;
    }
    if (offset != NULL) {
        *offset = rest;
    }
    return METER_MAP_OK;
}

MeterMapResult meter_map_snap_to_beat(const MeterMap *map, int measure, Rational time, Rational *snapped) {
    if (snapped == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    Rational start;
    Rational end;
    Rational beat;
    TRY(meter_map_measure_bounds(map, measure, &start, &end));
    TRY(meter_map_beat_length(map, measure, &beat));
    if (rational_cmp(time, start) <= 0) {
        *snapped = start;
        return METER_MAP_OK;
    }
    Rational offset;
    Rational steps;
    Rational advance;
    Rational candidate;
    long long count = 0;
    TRY(from_rational(rational_sub(time, start, &offset)));
    TRY(from_rational(rational_div(offset, beat, &steps)));
    TRY(from_rational(rational_ceil(steps, &count)));
    TRY(from_rational(rational_mul_int(beat, count, &advance)));
    TRY(from_rational(rational_add(start, advance, &candidate)));
    *snapped = rational_cmp(candidate, end) > 0 ? end : candidate;
    return METER_MAP_OK;
}

bool meter_map_on_beat(const MeterMap *map, Rational time) {
    int measure = 0;
    Rational offset;
    Rational length;
    Rational steps;
    if (meter_map_beat_of(map, time, &measure, NULL, &offset) != METER_MAP_OK ||
        meter_map_beat_length(map, measure, &length) != METER_MAP_OK ||
        rational_div(offset, length, &steps) != RATIONAL_OK) {
        return false;
    }
    return steps.den == 1;
}

int meter_map_first_counted_measure(const MeterMap *map) {
    if (map == NULL) {
        return 1;
    }
    return meter_map_has_pickup(map) ? 1 : meter_map_first_measure_number(map);
}
