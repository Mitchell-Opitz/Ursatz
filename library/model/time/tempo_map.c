#include "model/time/tempo_map.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/rational.h"
#include "model/time/time_rational.h"

/* Exact arithmetic is core/rational.h; every operation is checked and reports overflow. */
static int rat_of_time(const MusicalTime *time, Rational *out) {
    return rational_of_musical_time(time, out) == RATIONAL_OK ? 0 : -1;
}

static int fits_time(Rational r) {
    return r.num >= 0 && r.num <= (long long)LONG_MAX && (unsigned long long)r.den <= ULONG_MAX;
}

struct TempoMapRegion {
    EntityID *id;
    MusicalTime *start;
    MusicalTime *end; /* NULL while open */
    Rational start_r;
    Rational end_r; /* meaningful only when !open */
    bool open;
    unsigned long microseconds_per_quarter;
    Rational seconds_per_whole_note; /* 4 * T / 1,000,000 */
    Rational seconds_at_start;
    bool defaulted;
};

struct TempoMap {
    EntityID *id;
    TempoMapRegion *regions;
    size_t count;
};

static MusicalTime *time_of_rat(Rational r) {
    MusicalTime *t = NULL;
    if (!fits_time(r) || musical_time_create((long)r.num, (unsigned long)r.den, &t) != MUSICAL_TIME_OK) {
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

static TempoMapResult region_fill(TempoMapRegion *region, Rational start, unsigned long microseconds_per_quarter,
                                   bool defaulted) {
    Rational seconds_per_whole_note;
    if (microseconds_per_quarter > (unsigned long)(LLONG_MAX / 4) ||
        rational_make(4LL * (long long)microseconds_per_quarter, 1000000LL, &seconds_per_whole_note) != RATIONAL_OK) {
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
    region->seconds_per_whole_note = seconds_per_whole_note;
    region->defaulted = defaulted;
    return TEMPO_MAP_OK;
}

/* Closes each region at the next one's start, accumulates the seconds at each start, and mints the
   region ids. */
static TempoMapResult finalize(TempoMap *map) {
    Rational running = {0, 1};
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
            Rational length = {0, 1};
            Rational seconds = {0, 1};
            if (rational_sub(region->end_r, region->start_r, &length) != 0 ||
                rational_mul(length, region->seconds_per_whole_note, &seconds) != 0 ||
                rational_add(running, seconds, &running) != 0) {
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
        if (i > 0) {
            Rational previous;
            Rational current;
            if (rat_of_time(change_points[i - 1].start, &previous) != 0 ||
                rat_of_time(change_points[i].start, &current) != 0) {
                return TEMPO_MAP_ERR_OVERFLOW;
            }
            if (rational_cmp(previous, current) >= 0) {
                return TEMPO_MAP_ERR_NOT_ASCENDING;
            }
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

    Rational zero = {0, 1};
    Rational first;
    if (rat_of_time(change_points[0].start, &first) != 0) {
        map_release(map, 0);
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    if (rational_cmp(first, zero) != 0) {
        TempoMapResult filled = region_fill(&map->regions[0], zero, TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER, true);
        if (filled != TEMPO_MAP_OK) {
            map_release(map, 0);
            return filled;
        }
        map->count = 1;
    }

    for (size_t i = 0; i < count; i++) {
        Rational start;
        if (rat_of_time(change_points[i].start, &start) != 0) {
            map_release(map, map->count);
            return TEMPO_MAP_ERR_OVERFLOW;
        }
        TempoMapResult filled = region_fill(&map->regions[map->count], start,
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
    Rational zero = {0, 1};
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

const MusicalTime *tempo_map_region_start(const TempoMapRegion *region) {
    return region->start;
}

unsigned long tempo_map_region_microseconds_per_quarter(const TempoMapRegion *region) {
    return region->microseconds_per_quarter;
}

bool tempo_map_region_is_defaulted(const TempoMapRegion *region) {
    return region->defaulted;
}
