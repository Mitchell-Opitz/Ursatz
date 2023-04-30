#include "time/tempo_map.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact non-negative rational on unsigned 64-bit integers, always in lowest terms. Every operation
   is checked and returns -1 on overflow: nothing here wraps. */
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

static int rat_mul(Rat a, Rat b, Rat *out) {
    /* Cross-reduce first so the products are as small as they can be. */
    unsigned long long g1 = gcd_ull(a.n, b.d);
    unsigned long long g2 = gcd_ull(b.n, a.d);
    unsigned long long an = g1 > 1 ? a.n / g1 : a.n;
    unsigned long long bd = g1 > 1 ? b.d / g1 : b.d;
    unsigned long long bn = g2 > 1 ? b.n / g2 : b.n;
    unsigned long long ad = g2 > 1 ? a.d / g2 : a.d;
    unsigned long long n = 0;
    unsigned long long d = 0;
    if (mul_checked(an, bn, &n) != 0 || mul_checked(ad, bd, &d) != 0) {
        return -1;
    }
    *out = rat_reduced(n, d);
    return 0;
}

/* a / b, for b > 0. */
static int rat_div(Rat a, Rat b, Rat *out) {
    Rat inverse = {b.d, b.n};
    return rat_mul(a, inverse, out);
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

/* -1, 0, 1. Compares the integer parts and then the reciprocals of the remainders, as in Euclid's
   algorithm, so it never multiplies and cannot overflow. */
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

static Rat rat_of_performance_time(const PerformanceTime *time) {
    return rat_reduced((unsigned long long)performance_time_numerator(time), performance_time_denominator(time));
}

static int fits_time(Rat r) {
    return r.n <= (unsigned long long)LONG_MAX && r.d <= (unsigned long long)ULONG_MAX;
}

struct TempoMapRegion {
    EntityID *id;
    MusicalTime *start;
    MusicalTime *end; /* NULL while open */
    Rat start_r;
    Rat end_r; /* meaningful only when !open */
    bool open;
    unsigned long microseconds_per_quarter;
    Rat seconds_per_whole_note; /* 4 * T / 1,000,000 */
    Rat seconds_at_start;
    bool defaulted;
};

struct TempoMap {
    EntityID *id;
    TempoMapRegion *regions;
    size_t count;
};

static MusicalTime *time_of_rat(Rat r) {
    MusicalTime *t = NULL;
    if (!fits_time(r) || musical_time_create((long)r.n, (unsigned long)r.d, &t) != MUSICAL_TIME_OK) {
        return NULL;
    }
    return t;
}

static void region_release(TempoMapRegion *region) {
    entity_id_destroy(region->id);
    musical_time_destroy(region->start);
    musical_time_destroy(region->end);
}

static void map_release(TempoMap *map, size_t filled) {
    for (size_t i = 0; i < filled; i++) {
        region_release(&map->regions[i]);
    }
    free(map->regions);
    free(map);
}

static TempoMapResult region_fill(TempoMapRegion *region, Rat start, unsigned long microseconds_per_quarter,
                                   bool defaulted) {
    unsigned long long four_t = 0;
    if (mul_checked(4ULL, (unsigned long long)microseconds_per_quarter, &four_t) != 0) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    MusicalTime *start_time = time_of_rat(start);
    if (start_time == NULL) {
        return TEMPO_MAP_ERR_ALLOC;
    }
    memset(region, 0, sizeof(*region));
    region->start = start_time;
    region->start_r = start;
    region->open = true;
    region->microseconds_per_quarter = microseconds_per_quarter;
    region->seconds_per_whole_note = rat_reduced(four_t, 1000000ULL);
    region->defaulted = defaulted;
    return TEMPO_MAP_OK;
}

/* Closes each region at the next one's start, accumulates the seconds at each start, and mints the
   region ids. */
static TempoMapResult finalize(TempoMap *map) {
    Rat running = {0, 1};
    for (size_t i = 0; i < map->count; i++) {
        TempoMapRegion *region = &map->regions[i];
        region->seconds_at_start = running;

        if (i + 1 < map->count) {
            region->end_r = map->regions[i + 1].start_r;
            region->open = false;
            region->end = time_of_rat(region->end_r);
            if (region->end == NULL) {
                return TEMPO_MAP_ERR_ALLOC;
            }
            Rat length = {0, 1};
            Rat seconds = {0, 1};
            if (rat_sub(region->end_r, region->start_r, &length) != 0 ||
                rat_mul(length, region->seconds_per_whole_note, &seconds) != 0 ||
                rat_add(running, seconds, &running) != 0) {
                return TEMPO_MAP_ERR_OVERFLOW;
            }
        }

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s/region-%zu", entity_id_value(map->id), i);
        if (entity_id_create(buffer, &region->id) != ENTITY_ID_OK) {
            return TEMPO_MAP_ERR_ALLOC;
        }
    }
    return TEMPO_MAP_OK;
}

TempoMapResult tempo_map_create(EntityID *id, const TempoMapChangePoint *change_points, size_t count, TempoMap **out) {
    if (id == NULL || out == NULL || (change_points == NULL && count > 0)) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    if (count == 0) {
        return TEMPO_MAP_ERR_NO_CHANGE_POINTS;
    }

    for (size_t i = 0; i < count; i++) {
        if (change_points[i].start == NULL) {
            return TEMPO_MAP_ERR_NULL_ARGUMENT;
        }
        if (change_points[i].microseconds_per_quarter == 0) {
            return TEMPO_MAP_ERR_ZERO_TEMPO;
        }
        if (musical_time_numerator(change_points[i].start) < 0) {
            return TEMPO_MAP_ERR_NEGATIVE_START;
        }
        if (i > 0 && rat_cmp(rat_of_time(change_points[i - 1].start), rat_of_time(change_points[i].start)) >= 0) {
            return TEMPO_MAP_ERR_NOT_ASCENDING;
        }
    }

    TempoMap *map = (TempoMap *)calloc(1, sizeof(TempoMap));
    if (map == NULL) {
        return TEMPO_MAP_ERR_ALLOC;
    }
    map->regions = (TempoMapRegion *)calloc(count + 1, sizeof(TempoMapRegion));
    if (map->regions == NULL) {
        free(map);
        return TEMPO_MAP_ERR_ALLOC;
    }

    Rat zero = {0, 1};
    if (rat_cmp(rat_of_time(change_points[0].start), zero) != 0) {
        TempoMapResult filled = region_fill(&map->regions[0], zero, TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER, true);
        if (filled != TEMPO_MAP_OK) {
            map_release(map, 0);
            return filled;
        }
        map->count = 1;
    }

    for (size_t i = 0; i < count; i++) {
        TempoMapResult filled = region_fill(&map->regions[map->count], rat_of_time(change_points[i].start),
                                            change_points[i].microseconds_per_quarter, false);
        if (filled != TEMPO_MAP_OK) {
            map_release(map, map->count);
            return filled;
        }
        map->count++;
    }

    map->id = id;
    TempoMapResult finalized = finalize(map);
    if (finalized != TEMPO_MAP_OK) {
        map->id = NULL;
        map_release(map, map->count);
        return finalized;
    }

    *out = map;
    return TEMPO_MAP_OK;
}

TempoMapResult tempo_map_create_defaulted(EntityID *id, TempoMap **out) {
    if (id == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    TempoMap *map = (TempoMap *)calloc(1, sizeof(TempoMap));
    if (map == NULL) {
        return TEMPO_MAP_ERR_ALLOC;
    }
    map->regions = (TempoMapRegion *)calloc(1, sizeof(TempoMapRegion));
    if (map->regions == NULL) {
        free(map);
        return TEMPO_MAP_ERR_ALLOC;
    }
    Rat zero = {0, 1};
    TempoMapResult filled = region_fill(&map->regions[0], zero, TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER, true);
    if (filled != TEMPO_MAP_OK) {
        map_release(map, 0);
        return filled;
    }
    map->count = 1;
    map->id = id;
    TempoMapResult finalized = finalize(map);
    if (finalized != TEMPO_MAP_OK) {
        map->id = NULL;
        map_release(map, map->count);
        return finalized;
    }
    *out = map;
    return TEMPO_MAP_OK;
}

void tempo_map_destroy(TempoMap *map) {
    if (map == NULL) {
        return;
    }
    entity_id_destroy(map->id);
    map_release(map, map->count);
}

const EntityID *tempo_map_id(const TempoMap *map) {
    return map->id;
}

bool tempo_map_is_defaulted(const TempoMap *map) {
    for (size_t i = 0; i < map->count; i++) {
        if (!map->regions[i].defaulted) {
            return false;
        }
    }
    return true;
}

size_t tempo_map_region_count(const TempoMap *map) {
    return map->count;
}

const TempoMapRegion *tempo_map_region(const TempoMap *map, size_t index) {
    return index < map->count ? &map->regions[index] : NULL;
}

/* The last region that starts at or before t, for t >= 0. */
static const TempoMapRegion *find_region(const TempoMap *map, Rat t) {
    for (size_t i = map->count; i > 0; i--) {
        if (rat_cmp(map->regions[i - 1].start_r, t) <= 0) {
            return &map->regions[i - 1];
        }
    }
    return &map->regions[0];
}

TempoMapResult tempo_map_region_at(const TempoMap *map, const MusicalTime *time, const TempoMapRegion **out) {
    if (map == NULL || time == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    if (musical_time_numerator(time) < 0) {
        return TEMPO_MAP_ERR_TIME_BEFORE_START;
    }
    *out = find_region(map, rat_of_time(time));
    return TEMPO_MAP_OK;
}

TempoMapResult tempo_map_region_by_id(const TempoMap *map, const EntityID *id, const TempoMapRegion **out) {
    if (map == NULL || id == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < map->count; i++) {
        if (entity_id_equals(map->regions[i].id, id)) {
            *out = &map->regions[i];
            return TEMPO_MAP_OK;
        }
    }
    return TEMPO_MAP_ERR_NOT_FOUND;
}

TempoMapResult tempo_map_tempo_at(const TempoMap *map, const MusicalTime *time,
                                   unsigned long *out_microseconds_per_quarter) {
    if (out_microseconds_per_quarter == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    const TempoMapRegion *region = NULL;
    TempoMapResult found = tempo_map_region_at(map, time, &region);
    if (found != TEMPO_MAP_OK) {
        return found;
    }
    *out_microseconds_per_quarter = region->microseconds_per_quarter;
    return TEMPO_MAP_OK;
}

static TempoMapResult performance_time_of_rat(Rat r, PerformanceTime **out) {
    if (r.n > (unsigned long long)LONG_MAX || r.d > (unsigned long long)ULONG_MAX) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    PerformanceTimeResult created = performance_time_create((long)r.n, (unsigned long)r.d, out);
    if (created == PERFORMANCE_TIME_OK) {
        return TEMPO_MAP_OK;
    }
    return created == PERFORMANCE_TIME_ERR_OVERFLOW ? TEMPO_MAP_ERR_OVERFLOW : TEMPO_MAP_ERR_ALLOC;
}

/* Seconds from 0 to t (t >= 0), as a Rat. */
static TempoMapResult seconds_rat_at(const TempoMap *map, Rat t, Rat *out) {
    const TempoMapRegion *region = find_region(map, t);
    Rat into = {0, 1};
    Rat seconds = {0, 1};
    if (rat_sub(t, region->start_r, &into) != 0 || rat_mul(into, region->seconds_per_whole_note, &seconds) != 0 ||
        rat_add(region->seconds_at_start, seconds, out) != 0) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    return TEMPO_MAP_OK;
}

TempoMapResult tempo_map_seconds_at(const TempoMap *map, const MusicalTime *time, PerformanceTime **out) {
    if (map == NULL || time == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    if (musical_time_numerator(time) < 0) {
        return TEMPO_MAP_ERR_TIME_BEFORE_START;
    }
    Rat seconds = {0, 1};
    TempoMapResult computed = seconds_rat_at(map, rat_of_time(time), &seconds);
    if (computed != TEMPO_MAP_OK) {
        return computed;
    }
    return performance_time_of_rat(seconds, out);
}

TempoMapResult tempo_map_time_at_seconds(const TempoMap *map, const PerformanceTime *seconds, MusicalTime **out) {
    if (map == NULL || seconds == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    Rat s = rat_of_performance_time(seconds);

    /* The last region whose seconds at start are at or before s. */
    const TempoMapRegion *region = &map->regions[0];
    for (size_t i = map->count; i > 0; i--) {
        if (rat_cmp(map->regions[i - 1].seconds_at_start, s) <= 0) {
            region = &map->regions[i - 1];
            break;
        }
    }

    Rat into = {0, 1};
    Rat whole_notes = {0, 1};
    Rat time = {0, 1};
    if (rat_sub(s, region->seconds_at_start, &into) != 0 ||
        rat_div(into, region->seconds_per_whole_note, &whole_notes) != 0 ||
        rat_add(region->start_r, whole_notes, &time) != 0) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    if (!fits_time(time)) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    MusicalTime *result = time_of_rat(time);
    if (result == NULL) {
        return TEMPO_MAP_ERR_ALLOC;
    }
    *out = result;
    return TEMPO_MAP_OK;
}

TempoMapResult tempo_map_duration_seconds(const TempoMap *map, const TimeSpan *span, PerformanceTime **out) {
    if (map == NULL || span == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    const MusicalTime *start = time_span_start(span);
    const MusicalTime *end = time_span_end(span);
    if (musical_time_numerator(start) < 0) {
        return TEMPO_MAP_ERR_TIME_BEFORE_START;
    }
    Rat at_start = {0, 1};
    Rat at_end = {0, 1};
    Rat length = {0, 1};
    TempoMapResult computed = seconds_rat_at(map, rat_of_time(start), &at_start);
    if (computed == TEMPO_MAP_OK) {
        computed = seconds_rat_at(map, rat_of_time(end), &at_end);
    }
    if (computed != TEMPO_MAP_OK) {
        return computed;
    }
    if (rat_sub(at_end, at_start, &length) != 0) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    return performance_time_of_rat(length, out);
}

const EntityID *tempo_map_region_id(const TempoMapRegion *region) {
    return region->id;
}

const MusicalTime *tempo_map_region_start(const TempoMapRegion *region) {
    return region->start;
}

const MusicalTime *tempo_map_region_end(const TempoMapRegion *region) {
    return region->end;
}

unsigned long tempo_map_region_microseconds_per_quarter(const TempoMapRegion *region) {
    return region->microseconds_per_quarter;
}

bool tempo_map_region_is_defaulted(const TempoMapRegion *region) {
    return region->defaulted;
}

TempoMapResult tempo_map_region_seconds_at_start(const TempoMapRegion *region, PerformanceTime **out) {
    if (region == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    return performance_time_of_rat(region->seconds_at_start, out);
}
