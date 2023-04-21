#include "time/metrical_position.h"

#include <limits.h>
#include <stdlib.h>

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

/* Exact non-negative rational on unsigned 64-bit integers, in lowest terms. Every operation is
   checked and returns -1 on overflow. */
typedef struct {
    unsigned long long n;
    unsigned long long d;
} Rat;

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static int mul_checked(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ULLONG_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static int add_checked(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (b > ULLONG_MAX - a) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static Rat rat_reduced(unsigned long long n, unsigned long long d) {
    unsigned long long g = gcd_ull(n, d);
    Rat r;
    r.n = g > 1 ? n / g : n;
    r.d = g > 1 ? d / g : d;
    return r;
}

static int rat_add(Rat a, Rat b, Rat *out) {
    unsigned long long g = gcd_ull(a.d, b.d);
    unsigned long long ad_over_g = a.d / g;
    unsigned long long bd_over_g = b.d / g;
    unsigned long long left = 0;
    unsigned long long right = 0;
    unsigned long long n = 0;
    unsigned long long d = 0;
    if (mul_checked(a.n, bd_over_g, &left) != 0 || mul_checked(b.n, ad_over_g, &right) != 0 ||
        add_checked(left, right, &n) != 0 || mul_checked(ad_over_g, b.d, &d) != 0) {
        return -1;
    }
    *out = rat_reduced(n, d);
    return 0;
}

/* a - b, for a >= b. */
static int rat_sub(Rat a, Rat b, Rat *out) {
    unsigned long long g = gcd_ull(a.d, b.d);
    unsigned long long ad_over_g = a.d / g;
    unsigned long long bd_over_g = b.d / g;
    unsigned long long left = 0;
    unsigned long long right = 0;
    unsigned long long d = 0;
    if (mul_checked(a.n, bd_over_g, &left) != 0 || mul_checked(b.n, ad_over_g, &right) != 0 ||
        mul_checked(ad_over_g, b.d, &d) != 0) {
        return -1;
    }
    *out = rat_reduced(left - right, d);
    return 0;
}

/* -1, 0, 1, by comparing integer parts and then reciprocals of remainders (never multiplies). */
static int rat_cmp(Rat x, Rat y) {
    unsigned long long a = x.n;
    unsigned long long b = x.d;
    unsigned long long c = y.n;
    unsigned long long d = y.d;
    int sign = 1;
    for (;;) {
        unsigned long long qa = a / b;
        unsigned long long qc = c / d;
        if (qa != qc) {
            return qa < qc ? -sign : sign;
        }
        unsigned long long ra = a % b;
        unsigned long long rc = c % d;
        if (ra == 0 && rc == 0) {
            return 0;
        }
        if (ra == 0) {
            return -sign;
        }
        if (rc == 0) {
            return sign;
        }
        a = b;
        b = ra;
        c = d;
        d = rc;
        sign = -sign;
    }
}

static Rat rat_of_time(const MusicalTime *time) {
    return rat_reduced((unsigned long long)musical_time_numerator(time), musical_time_denominator(time));
}

static Rat rat_of_duration(const Duration *duration) {
    return rat_reduced((unsigned long long)duration_numerator(duration), duration_denominator(duration));
}

static int fits(Rat r) {
    return r.n <= (unsigned long long)LONG_MAX && r.d <= (unsigned long long)ULONG_MAX;
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
        Rat length = {0, 1};
        Rat start = rat_of_time(meter_map_region_start(region));
        Rat end = rat_of_time(meter_map_region_end(region));
        int failed = rat_sub(end, start, &length);
        int is_short_leading_signature = failed == 0 && rat_cmp(length, rat_of_duration(own)) >= 0;
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
static MetricalPositionResult pickup_base(const MeterMap *map, Rat span_start, Rat span_end, Rat *out_base,
                                           Rat *out_full) {
    const TimeSignature *reading = NULL;
    MetricalPositionResult got = metrical_position_reading_signature(map, 0, &reading);
    if (got != METRICAL_POSITION_OK) {
        return got;
    }
    Duration *measure = NULL;
    if (time_signature_measure_duration(reading, &measure) != DURATION_OK) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    Rat full = rat_of_duration(measure);
    duration_destroy(measure);

    Rat length = {0, 1};
    if (rat_sub(span_end, span_start, &length) != 0 || rat_cmp(length, full) > 0 ||
        rat_sub(full, length, out_base) != 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    *out_full = full;
    return METRICAL_POSITION_OK;
}

static MetricalPositionResult span_bounds(const MeterMap *map, int measure, Rat *out_start, Rat *out_end) {
    TimeSpan *span = NULL;
    MeterMapResult made = meter_map_measure_span(map, measure, &span);
    if (made != METER_MAP_OK) {
        return meter_map_failure(made);
    }
    *out_start = rat_of_time(time_span_start(span));
    *out_end = rat_of_time(time_span_end(span));
    time_span_destroy(span);
    return METRICAL_POSITION_OK;
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

    Rat start = {0, 1};
    Rat end = {0, 1};
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
    Rat t = rat_of_time(time);
    if (rat_cmp(t, start) < 0 || rat_cmp(t, end) >= 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }

    Rat offset = {0, 1};
    if (rat_sub(t, start, &offset) != 0) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    if (measure == 0 && meter_map_has_pickup(map)) {
        Rat base = {0, 1};
        Rat full = {0, 1};
        MetricalPositionResult based = pickup_base(map, start, end, &base, &full);
        if (based != METRICAL_POSITION_OK) {
            return based;
        }
        if (rat_add(base, offset, &offset) != 0) {
            return METRICAL_POSITION_ERR_OVERFLOW;
        }
    }

    if (!fits(offset)) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    Duration *offset_duration = NULL;
    if (duration_create((long)offset.n, (unsigned long)offset.d, &offset_duration) != DURATION_OK) {
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
    Rat start = {0, 1};
    Rat end = {0, 1};
    MetricalPositionResult bounded = span_bounds(map, measure, &start, &end);
    if (bounded != METRICAL_POSITION_OK) {
        return bounded;
    }

    Rat offset = rat_of_duration(metrical_position_offset(position));
    Rat into_span = offset; /* how far past the measure's start in time */
    if (measure == 0 && meter_map_has_pickup(map)) {
        Rat base = {0, 1};
        Rat full = {0, 1};
        MetricalPositionResult based = pickup_base(map, start, end, &base, &full);
        if (based != METRICAL_POSITION_OK) {
            return based;
        }
        if (rat_cmp(offset, base) < 0 || rat_cmp(offset, full) >= 0 || rat_sub(offset, base, &into_span) != 0) {
            return METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE;
        }
    } else {
        Rat length = {0, 1};
        if (rat_sub(end, start, &length) != 0) {
            return METRICAL_POSITION_ERR_OVERFLOW;
        }
        if (rat_cmp(offset, length) >= 0) {
            return METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE;
        }
    }

    Rat time = {0, 1};
    if (rat_add(start, into_span, &time) != 0 || !fits(time)) {
        return METRICAL_POSITION_ERR_OVERFLOW;
    }
    if (musical_time_create((long)time.n, (unsigned long)time.d, out) != MUSICAL_TIME_OK) {
        return METRICAL_POSITION_ERR_ALLOC;
    }
    return METRICAL_POSITION_OK;
}
