#include "time/metrical_position.h"

#include <limits.h>
#include <stdlib.h>
#include "core/rational.h"
#include "time/time_rational.h"

struct MetricalPosition {
    int measure;
    Duration *offset;
};

MetricalPositionResult metrical_position_create(int measure, const Duration *offset, MetricalPosition **out) {
    if (offset == NULL || out == NULL) {
        return METRICAL_POSITION_ERR_NULL_ARGUMENT;
    }

    MetricalPosition *position = (MetricalPosition *)malloc(sizeof(MetricalPosition));
    if (position == NULL) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    if (duration_create(duration_numerator(offset), duration_denominator(offset), &position->offset) != DURATION_OK) {
        free(position);
        return METRICAL_POSITION_ERR_ALLOC;
    }
    position->measure = measure;

    *out = position;
    return METRICAL_POSITION_OK;
}

void metrical_position_destroy(MetricalPosition *position) {
    if (position == NULL) {
        return;
    }
    duration_destroy(position->offset);
    free(position);
}

int metrical_position_measure(const MetricalPosition *position) {
    return position->measure;
}

const Duration *metrical_position_offset(const MetricalPosition *position) {
    return position->offset;
}

bool metrical_position_equals(const MetricalPosition *a, const MetricalPosition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->measure == b->measure && duration_equals(a->offset, b->offset);
}

/* Exact arithmetic is core/rational.h; every operation is checked and reports overflow. */
static int rat_of_time(const MusicalTime *time, Rational *out) {
    return rational_of_musical_time(time, out) == RATIONAL_OK ? 0 : -1;
}

static int rat_of_duration(const Duration *duration, Rational *out) {
    return rational_of_duration(duration, out) == RATIONAL_OK ? 0 : -1;
}

static int fits(Rational r) {
    return r.num >= 0 && r.num <= (long long)LONG_MAX && (unsigned long long)r.den <= ULONG_MAX;
}

static MetricalPositionResult meter_map_failure(MeterMapResult result) {
    switch (result) {
    case METER_MAP_ERR_TIME_BEFORE_START:
        return METRICAL_POSITION_ERR_TIME_BEFORE_START;
    case METER_MAP_ERR_INDEX_OUT_OF_RANGE:
        return METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE;
    case METER_MAP_ERR_ALLOC:
        return METRICAL_POSITION_ERR_ALLOC;
    default:
        return METRICAL_POSITION_ERR_NULL_ARGUMENT;
    }
}

MetricalPositionResult metrical_position_reading_signature(const MeterMap *map, int measure,
                                                            const TimeSignature **out) {
    if (map == NULL || out == NULL) {
        return METRICAL_POSITION_ERR_NULL_ARGUMENT;
    }
    const MeterMapRegion *region = NULL;
    MeterMapResult found = meter_map_region_of_measure(map, measure, &region);
    if (found != METER_MAP_OK) {
        return meter_map_failure(found);
    }

    const TimeSignature *signature = meter_map_region_time_signature(region);
    if (meter_map_region_is_pickup(region) && meter_map_region_count(map) >= 2) {
        /* A pickup region that is exactly one measure of its own signature is a short leading
           signature, and the full measure it is the end of is the next region's. One shorter than
           its own measure is a partial first measure of its own signature. */
        Duration *own = NULL;
        if (time_signature_measure_duration(signature, &own) != DURATION_OK) {
            return METRICAL_POSITION_ERR_ALLOC;
        }
        Rational length = {0, 1};
        Rational start = {0, 1};
        Rational end = {0, 1};
        Rational own_length = {0, 1};
        int failed = rat_of_time(meter_map_region_start(region), &start) != 0 ||
                     rat_of_time(meter_map_region_end(region), &end) != 0 ||
                     rat_of_duration(own, &own_length) != 0 || rational_sub(end, start, &length) != RATIONAL_OK;
        int is_short_leading_signature = failed == 0 && rational_cmp(length, own_length) >= 0;
        duration_destroy(own);
        if (failed != 0) {
            return METRICAL_POSITION_ERR_OVERFLOW;
        }
        if (is_short_leading_signature) {
            signature = meter_map_region_time_signature(meter_map_region(map, 1));
        }
    }
    *out = signature;
    return METRICAL_POSITION_OK;
}

/* For measure 0 of a pickup map: how far into a full measure the pickup starts (full measure length
   less the pickup's own length). */
static MetricalPositionResult pickup_base(const MeterMap *map, Rational span_start, Rational span_end, Rational *out_base,
                                           Rational *out_full) {
    const TimeSignature *reading = NULL;
    MetricalPositionResult got = metrical_position_reading_signature(map, 0, &reading);
    if (got != METRICAL_POSITION_OK) {
        return got;
    }
    Duration *measure = NULL;
    if (time_signature_measure_duration(reading, &measure) != DURATION_OK) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    Rational full = {0, 1};
    int full_failed = rat_of_duration(measure, &full) != 0;
    duration_destroy(measure);
    if (full_failed) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }

    Rational length = {0, 1};
    if (rational_sub(span_end, span_start, &length) != 0 || rational_cmp(length, full) > 0 ||
        rational_sub(full, length, out_base) != 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    *out_full = full;
    return METRICAL_POSITION_OK;
}

static MetricalPositionResult span_bounds(const MeterMap *map, int measure, Rational *out_start, Rational *out_end) {
    TimeSpan *span = NULL;
    MeterMapResult made = meter_map_measure_span(map, measure, &span);
    if (made != METER_MAP_OK) {
        return meter_map_failure(made);
    }
    int failed = rat_of_time(time_span_start(span), out_start) != 0 || rat_of_time(time_span_end(span), out_end) != 0;
    time_span_destroy(span);
    return failed ? METRICAL_POSITION_ERR_OVERFLOW : METRICAL_POSITION_OK;
}

MetricalPositionResult metrical_position_from_time(const MeterMap *map, const MusicalTime *time,
                                                    MetricalPosition **out) {
    if (map == NULL || time == NULL || out == NULL) {
        return METRICAL_POSITION_ERR_NULL_ARGUMENT;
    }
    if (musical_time_numerator(time) < 0) {
        return METRICAL_POSITION_ERR_TIME_BEFORE_START;
    }

    int measure = 0;
    MeterMapResult indexed = meter_map_measure_index_at(map, time, &measure);
    if (indexed != METER_MAP_OK) {
        return meter_map_failure(indexed);
    }

    Rational start = {0, 1};
    Rational end = {0, 1};
    MetricalPositionResult bounded = span_bounds(map, measure, &start, &end);
    if (bounded == METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE) {
        /* The index came from the map itself, so it can only be out of range if it wrapped. */
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    if (bounded != METRICAL_POSITION_OK) {
        return bounded;
    }

    /* The measure number is an int: a time so far out that the number wrapped lands in a measure that
       does not contain it. */
    Rational t = {0, 1};
    if (rat_of_time(time, &t) != 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    if (rational_cmp(t, start) < 0 || rational_cmp(t, end) >= 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }

    Rational offset = {0, 1};
    if (rational_sub(t, start, &offset) != 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    if (measure == 0 && meter_map_has_pickup(map)) {
        Rational base = {0, 1};
        Rational full = {0, 1};
        MetricalPositionResult based = pickup_base(map, start, end, &base, &full);
        if (based != METRICAL_POSITION_OK) {
            return based;
        }
        if (rational_add(base, offset, &offset) != 0) {
            return METRICAL_POSITION_ERR_OVERFLOW;
        }
    }

    if (!fits(offset)) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    Duration *offset_duration = NULL;
    if (duration_create((long)offset.num, (unsigned long)offset.den, &offset_duration) != DURATION_OK) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    MetricalPositionResult created = metrical_position_create(measure, offset_duration, out);
    duration_destroy(offset_duration);
    return created;
}

MetricalPositionResult metrical_position_to_time(const MeterMap *map, const MetricalPosition *position,
                                                  MusicalTime **out) {
    if (map == NULL || position == NULL || out == NULL) {
        return METRICAL_POSITION_ERR_NULL_ARGUMENT;
    }
    int measure = metrical_position_measure(position);
    Rational start = {0, 1};
    Rational end = {0, 1};
    MetricalPositionResult bounded = span_bounds(map, measure, &start, &end);
    if (bounded != METRICAL_POSITION_OK) {
        return bounded;
    }

    Rational offset = {0, 1};
    if (rat_of_duration(metrical_position_offset(position), &offset) != 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    Rational into_span = offset; /* how far past the measure's start in time */
    if (measure == 0 && meter_map_has_pickup(map)) {
        Rational base = {0, 1};
        Rational full = {0, 1};
        MetricalPositionResult based = pickup_base(map, start, end, &base, &full);
        if (based != METRICAL_POSITION_OK) {
            return based;
        }
        if (rational_cmp(offset, base) < 0 || rational_cmp(offset, full) >= 0 || rational_sub(offset, base, &into_span) != 0) {
            return METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE;
        }
    } else {
        Rational length = {0, 1};
        if (rational_sub(end, start, &length) != 0) {
            return METRICAL_POSITION_ERR_OVERFLOW;
        }
        if (rational_cmp(offset, length) >= 0) {
            return METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE;
        }
    }

    Rational time = {0, 1};
    if (rational_add(start, into_span, &time) != 0 || !fits(time)) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    if (musical_time_create((long)time.num, (unsigned long)time.den, out) != MUSICAL_TIME_OK) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    return METRICAL_POSITION_OK;
}
