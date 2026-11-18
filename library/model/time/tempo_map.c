#include "model/time/tempo_map.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/rational.h"

struct TempoMapRegion {
    EntityID *id;
    Rational start_r;
    Rational end_r; /* meaningful only when !open */
    bool open;
    unsigned long microseconds_per_quarter; /* the exact tempo rounded to a whole microsecond (a modulation can leave a fraction) */
    Rational micros_per_quarter;            /* the exact tempo: a modulation resolves against the previous region's */
    Rational seconds_per_whole_note;        /* 4 * T / 1,000,000 */
    Rational seconds_at_start;
    bool defaulted;
};

struct TempoMap {
    EntityID *id;
    TempoMapRegion *regions;
    size_t count;
};

static void region_release(TempoMapRegion *region) {
    entity_id_destroy(region->id);
}

static void map_release(TempoMap *map, size_t filled) {
    for (size_t i = 0; i < filled; i++) {
        region_release(&map->regions[i]);
    }
    free(map->regions);
    free(map);
}

/* A region of tempo `micros` (microseconds per quarter note, exact, above 0) from `start`. */
static TempoMapResult region_fill(TempoMapRegion *region, Rational start, Rational micros, bool defaulted) {
    Rational seconds_per_whole_note;
    Rational half = {1, 2};
    Rational rounded;
    long long whole = 0;
    if (rational_mul_int(micros, 4, &seconds_per_whole_note) != RATIONAL_OK ||
        rational_div_int(seconds_per_whole_note, 1000000, &seconds_per_whole_note) != RATIONAL_OK ||
        rational_add(micros, half, &rounded) != RATIONAL_OK || rational_floor(rounded, &whole) != RATIONAL_OK ||
        whole > LLONG_MAX / 4) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    memset(region, 0, sizeof(*region));
    region->start_r = start;
    region->open = true;
    region->microseconds_per_quarter = whole < 1 ? 1UL : (unsigned long)whole;
    region->micros_per_quarter = micros;
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
        if (change_points[i].kind == TEMPO_MAP_CHANGE_MODULATION) {
            Rational zero_value = {0, 1};
            if (change_points[i].old_value.den <= 0 || change_points[i].new_value.den <= 0 ||
                rational_cmp(change_points[i].old_value, zero_value) <= 0 ||
                rational_cmp(change_points[i].new_value, zero_value) <= 0) {
                return TEMPO_MAP_ERR_BAD_MODULATION;
            }
        } else if (change_points[i].microseconds_per_quarter == 0) {
            return TEMPO_MAP_ERR_ZERO_TEMPO;
        }
        if (change_points[i].start.value.num < 0) {
            return TEMPO_MAP_ERR_NEGATIVE_START;
        }
        if (i > 0) {
            Rational previous = change_points[i - 1].start.value;
            Rational current = change_points[i].start.value;
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
    Rational first = change_points[0].start.value;
    if (rational_cmp(first, zero) != 0) {
        TempoMapResult filled = region_fill(&map->regions[0], zero, rational_ratio(TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER, 1),
                                            true);
        if (filled != TEMPO_MAP_OK) {
            map_release(map, 0);
            return filled;
        }
        map->count = 1;
    }

    for (size_t i = 0; i < count; i++) {
        Rational start = change_points[i].start.value;
        Rational micros;
        if (change_points[i].kind == TEMPO_MAP_CHANGE_MODULATION) {
            /* the new tempo makes new_value last as long as old_value did at the tempo in force: T * old / new */
            Rational scaled;
            if (map->count == 0) {
                map_release(map, 0);
                return TEMPO_MAP_ERR_BAD_MODULATION; /* nothing to modulate from at the start of the piece */
            }
            if (rational_mul(map->regions[map->count - 1].micros_per_quarter, change_points[i].old_value, &scaled) !=
                    RATIONAL_OK ||
                rational_div(scaled, change_points[i].new_value, &micros) != RATIONAL_OK) {
                map_release(map, map->count);
                return TEMPO_MAP_ERR_OVERFLOW;
            }
        } else if (rational_from_int((long long)change_points[i].microseconds_per_quarter, &micros) != RATIONAL_OK ||
                   change_points[i].microseconds_per_quarter > (unsigned long)(LLONG_MAX / 4)) {
            map_release(map, map->count);
            return TEMPO_MAP_ERR_OVERFLOW;
        }
        TempoMapResult filled = region_fill(&map->regions[map->count], start, micros, false);
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
    TempoMapResult filled = region_fill(&map->regions[0], zero, rational_ratio(TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER, 1), true);
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

MusicalTime tempo_map_region_start(const TempoMapRegion *region) {
    return musical_time_of(region->start_r);
}

unsigned long tempo_map_region_microseconds_per_quarter(const TempoMapRegion *region) {
    return region->microseconds_per_quarter;
}

bool tempo_map_region_is_defaulted(const TempoMapRegion *region) {
    return region->defaulted;
}

/* The last region that starts at or before `time`. */
static const TempoMapRegion *region_at_time(const TempoMap *map, Rational time) {
    const TempoMapRegion *found = &map->regions[0];
    for (size_t i = 1; i < map->count; i++) {
        if (rational_cmp(map->regions[i].start_r, time) > 0) {
            break;
        }
        found = &map->regions[i];
    }
    return found;
}

/* The last region that has begun by `seconds`. */
static const TempoMapRegion *region_at_seconds(const TempoMap *map, Rational seconds) {
    const TempoMapRegion *found = &map->regions[0];
    for (size_t i = 1; i < map->count; i++) {
        if (rational_cmp(map->regions[i].seconds_at_start, seconds) > 0) {
            break;
        }
        found = &map->regions[i];
    }
    return found;
}

TempoMapResult tempo_map_seconds_at(const TempoMap *map, MusicalTime t, Rational *seconds) {
    if (map == NULL || seconds == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    if (rational_is_negative(t.value)) {
        return TEMPO_MAP_ERR_NEGATIVE_START;
    }
    const TempoMapRegion *region = region_at_time(map, t.value);
    Rational into;
    Rational spent;
    Rational total;
    if (rational_sub(t.value, region->start_r, &into) != RATIONAL_OK ||
        rational_mul(into, region->seconds_per_whole_note, &spent) != RATIONAL_OK ||
        rational_add(region->seconds_at_start, spent, &total) != RATIONAL_OK) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    *seconds = total;
    return TEMPO_MAP_OK;
}

TempoMapResult tempo_map_time_at(const TempoMap *map, Rational seconds, MusicalTime *out) {
    if (map == NULL || out == NULL) {
        return TEMPO_MAP_ERR_NULL_ARGUMENT;
    }
    if (rational_is_negative(seconds)) {
        return TEMPO_MAP_ERR_NEGATIVE_START;
    }
    const TempoMapRegion *region = region_at_seconds(map, seconds);
    Rational into;
    Rational span;
    Rational time;
    if (rational_sub(seconds, region->seconds_at_start, &into) != RATIONAL_OK ||
        rational_div(into, region->seconds_per_whole_note, &span) != RATIONAL_OK ||
        rational_add(region->start_r, span, &time) != RATIONAL_OK) {
        return TEMPO_MAP_ERR_OVERFLOW;
    }
    *out = musical_time_of(time);
    return TEMPO_MAP_OK;
}
