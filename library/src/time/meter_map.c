#include "time/meter_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "time/duration.h"
#include "time/note_value.h"

/* Exact non-negative-denominator rational used for the grid arithmetic. Always reduced, so the
   products below stay small for any realistic tick position. */
typedef struct {
    long long n;
    long long d;
} Rat;

static long long gcd_ll(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    while (b != 0) {
        long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static Rat rat_make(long long n, long long d) {
    long long g = gcd_ll(n, d);
    if (g > 1) {
        n /= g;
        d /= g;
    }
    Rat r = {n, d};
    return r;
}

static Rat rat_of_time(const MusicalTime *time) {
    return rat_make(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

static int rat_cmp(Rat a, Rat b) {
    long long left = a.n * b.d;
    long long right = b.n * a.d;
    return left < right ? -1 : (left > right ? 1 : 0);
}

static Rat rat_add(Rat a, Rat b) {
    return rat_make(a.n * b.d + b.n * a.d, a.d * b.d);
}

static Rat rat_sub(Rat a, Rat b) {
    return rat_make(a.n * b.d - b.n * a.d, a.d * b.d);
}

static Rat rat_mul_int(Rat a, long long k) {
    return rat_make(a.n * k, a.d);
}

static Rat rat_min(Rat a, Rat b) {
    return rat_cmp(a, b) <= 0 ? a : b;
}

/* a / b rounded down; a >= 0, b > 0. */
static long long rat_floor_div(Rat a, Rat b) {
    return (a.n * b.d) / (a.d * b.n);
}

/* a / b rounded up; a >= 0, b > 0. */
static long long rat_ceil_div(Rat a, Rat b) {
    long long num = a.n * b.d;
    long long den = a.d * b.n;
    return (num + den - 1) / den;
}

static bool rat_is_multiple(Rat a, Rat b) {
    return ((a.n * b.d) % (a.d * b.n)) == 0;
}

struct MeterMapRegion {
    EntityID *id;
    TimeSignature *time_signature;
    MusicalTime *start;
    MusicalTime *end; /* NULL while open */
    Rat start_r;
    Rat end_r; /* meaningful only when !open */
    bool open;
    Rat measure_len;
    int first_measure_number;
    long long measure_total; /* measures in the region; meaningful only when !open */
    bool defaulted;
    bool pickup;
};

struct MeterMap {
    EntityID *id;
    MeterMapRegion *regions;
    size_t count;
    bool pickup;
};

static MusicalTime *time_of_rat(Rat r) {
    MusicalTime *t = NULL;
    if (musical_time_create((long)r.n, (unsigned long)r.d, &t) != MUSICAL_TIME_OK) {
        return NULL;
    }
    return t;
}

static TimeSignature *copy_time_signature(const TimeSignature *source) {
    NoteValue *unit = NULL;
    if (note_value_create(time_signature_bottom_unit_exponent(source), &unit) != NOTE_VALUE_OK) {
        return NULL;
    }
    TimeSignature *copy = NULL;
    TimeSignatureResult r = time_signature_create(time_signature_top_number(source), unit, &copy);
    note_value_destroy(unit);
    return r == TIME_SIGNATURE_OK ? copy : NULL;
}

static TimeSignature *make_four_four(void) {
    NoteValue *quarter = NULL;
    if (note_value_create(2, &quarter) != NOTE_VALUE_OK) {
        return NULL;
    }
    TimeSignature *ts = NULL;
    TimeSignatureResult r = time_signature_create(4, quarter, &ts);
    note_value_destroy(quarter);
    return r == TIME_SIGNATURE_OK ? ts : NULL;
}

static void region_release(MeterMapRegion *region) {
    entity_id_destroy(region->id);
    time_signature_destroy(region->time_signature);
    musical_time_destroy(region->start);
    musical_time_destroy(region->end);
}

/* Takes ownership of time_signature on success only. */
static int region_fill(MeterMapRegion *region, TimeSignature *time_signature, Rat start, bool defaulted) {
    Duration *measure = NULL;
    if (time_signature_measure_duration(time_signature, &measure) != DURATION_OK) {
        return -1;
    }
    Rat measure_len = rat_make(duration_numerator(measure), (long long)duration_denominator(measure));
    duration_destroy(measure);

    MusicalTime *start_time = time_of_rat(start);
    if (start_time == NULL) {
        return -1;
    }

    memset(region, 0, sizeof(*region));
    region->time_signature = time_signature;
    region->start = start_time;
    region->start_r = start;
    region->open = true;
    region->measure_len = measure_len;
    region->defaulted = defaulted;
    return 0;
}

static void map_release(MeterMap *map, size_t filled) {
    for (size_t i = 0; i < filled; i++) {
        region_release(&map->regions[i]);
    }
    free(map->regions);
    free(map);
}

/* Closes each region at the next one's start, decides the pickup, numbers the measures and mints
   the region ids. Returns 0, or -1 on allocation failure (regions already filled stay owned by the
   map, which the caller releases). */
static int finalize(MeterMap *map) {
    for (size_t i = 0; i + 1 < map->count; i++) {
        MeterMapRegion *region = &map->regions[i];
        region->end_r = map->regions[i + 1].start_r;
        region->open = false;
        region->end = time_of_rat(region->end_r);
        if (region->end == NULL) {
            return -1;
        }
    }

    if (map->count >= 2) {
        const MeterMapRegion *first = &map->regions[0];
        const MeterMapRegion *second = &map->regions[1];
        Rat length = rat_sub(first->end_r, first->start_r);
        int against_own = rat_cmp(length, first->measure_len);
        if (against_own < 0) {
            map->pickup = true;
        } else if (against_own == 0 &&
                   !time_signature_equals(first->time_signature, second->time_signature) &&
                   rat_cmp(rat_mul_int(first->measure_len, 2), second->measure_len) < 0) {
            map->pickup = true;
        }
    }
    map->regions[0].pickup = map->pickup;

    int number = map->pickup ? 0 : 1;
    for (size_t i = 0; i < map->count; i++) {
        MeterMapRegion *region = &map->regions[i];
        region->first_measure_number = number;
        if (!region->open) {
            region->measure_total = rat_ceil_div(rat_sub(region->end_r, region->start_r), region->measure_len);
            number += (int)region->measure_total;
        }

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s/region-%zu", entity_id_value(map->id), i);
        if (entity_id_create(buffer, &region->id) != ENTITY_ID_OK) {
            return -1;
        }
    }
    return 0;
}

MeterMapResult meter_map_create(EntityID *id, const MeterMapChangePoint *change_points, size_t count, MeterMap **out) {
    if (id == NULL || out == NULL || (change_points == NULL && count > 0)) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    if (count == 0) {
        return METER_MAP_ERR_NO_CHANGE_POINTS;
    }

    for (size_t i = 0; i < count; i++) {
        if (change_points[i].time_signature == NULL || change_points[i].start == NULL) {
            return METER_MAP_ERR_NULL_ARGUMENT;
        }
        if (musical_time_numerator(change_points[i].start) < 0) {
            return METER_MAP_ERR_NEGATIVE_START;
        }
        if (i > 0 && rat_cmp(rat_of_time(change_points[i - 1].start), rat_of_time(change_points[i].start)) >= 0) {
            return METER_MAP_ERR_NOT_ASCENDING;
        }
    }

    MeterMap *map = (MeterMap *)calloc(1, sizeof(MeterMap));
    if (map == NULL) {
        return METER_MAP_ERR_ALLOC;
    }
    map->regions = (MeterMapRegion *)calloc(count + 1, sizeof(MeterMapRegion));
    if (map->regions == NULL) {
        free(map);
        return METER_MAP_ERR_ALLOC;
    }

    Rat zero = rat_make(0, 1);
    if (rat_cmp(rat_of_time(change_points[0].start), zero) != 0) {
        TimeSignature *default_signature = make_four_four();
        if (default_signature == NULL || region_fill(&map->regions[0], default_signature, zero, true) != 0) {
            time_signature_destroy(default_signature);
            map_release(map, 0);
            return METER_MAP_ERR_ALLOC;
        }
        map->count = 1;
    }

    for (size_t i = 0; i < count; i++) {
        Rat start = rat_of_time(change_points[i].start);
        if (map->count > 0) {
            const MeterMapRegion *last = &map->regions[map->count - 1];
            if (!last->defaulted && time_signature_equals(last->time_signature, change_points[i].time_signature) &&
                rat_is_multiple(rat_sub(start, last->start_r), last->measure_len)) {
                continue;
            }
        }
        TimeSignature *copy = copy_time_signature(change_points[i].time_signature);
        if (copy == NULL || region_fill(&map->regions[map->count], copy, start, false) != 0) {
            time_signature_destroy(copy);
            map_release(map, map->count);
            return METER_MAP_ERR_ALLOC;
        }
        map->count++;
    }

    /* finalize closes regions as it goes, and the ids come last, so a failure here leaves a
       partly built map that map_release frees region by region. */
    map->id = id;
    if (finalize(map) != 0) {
        map->id = NULL;
        map_release(map, map->count);
        return METER_MAP_ERR_ALLOC;
    }

    *out = map;
    return METER_MAP_OK;
}

MeterMapResult meter_map_create_defaulted(EntityID *id, MeterMap **out) {
    if (id == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    MeterMap *map = (MeterMap *)calloc(1, sizeof(MeterMap));
    if (map == NULL) {
        return METER_MAP_ERR_ALLOC;
    }
    map->regions = (MeterMapRegion *)calloc(1, sizeof(MeterMapRegion));
    if (map->regions == NULL) {
        free(map);
        return METER_MAP_ERR_ALLOC;
    }
    TimeSignature *default_signature = make_four_four();
    if (default_signature == NULL || region_fill(&map->regions[0], default_signature, rat_make(0, 1), true) != 0) {
        time_signature_destroy(default_signature);
        map_release(map, 0);
        return METER_MAP_ERR_ALLOC;
    }
    map->count = 1;
    map->id = id;
    if (finalize(map) != 0) {
        map->id = NULL;
        map_release(map, map->count);
        return METER_MAP_ERR_ALLOC;
    }
    *out = map;
    return METER_MAP_OK;
}

void meter_map_destroy(MeterMap *map) {
    if (map == NULL) {
        return;
    }
    entity_id_destroy(map->id);
    map_release(map, map->count);
}

const EntityID *meter_map_id(const MeterMap *map) {
    return map->id;
}

bool meter_map_is_defaulted(const MeterMap *map) {
    for (size_t i = 0; i < map->count; i++) {
        if (!map->regions[i].defaulted) {
            return false;
        }
    }
    return true;
}

bool meter_map_has_pickup(const MeterMap *map) {
    return map->pickup;
}

int meter_map_first_measure_number(const MeterMap *map) {
    return map->pickup ? 0 : 1;
}

size_t meter_map_region_count(const MeterMap *map) {
    return map->count;
}

const MeterMapRegion *meter_map_region(const MeterMap *map, size_t index) {
    return index < map->count ? &map->regions[index] : NULL;
}

/* The last region that starts at or before t, for t >= 0. */
static const MeterMapRegion *find_region(const MeterMap *map, Rat t) {
    for (size_t i = map->count; i > 0; i--) {
        if (rat_cmp(map->regions[i - 1].start_r, t) <= 0) {
            return &map->regions[i - 1];
        }
    }
    return &map->regions[0];
}

MeterMapResult meter_map_region_at(const MeterMap *map, const MusicalTime *time, const MeterMapRegion **out) {
    if (map == NULL || time == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    if (musical_time_numerator(time) < 0) {
        return METER_MAP_ERR_TIME_BEFORE_START;
    }
    *out = find_region(map, rat_of_time(time));
    return METER_MAP_OK;
}

MeterMapResult meter_map_region_by_id(const MeterMap *map, const EntityID *id, const MeterMapRegion **out) {
    if (map == NULL || id == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < map->count; i++) {
        if (entity_id_equals(map->regions[i].id, id)) {
            *out = &map->regions[i];
            return METER_MAP_OK;
        }
    }
    return METER_MAP_ERR_NOT_FOUND;
}

MeterMapResult meter_map_measure_index_at(const MeterMap *map, const MusicalTime *time, int *out_index) {
    if (map == NULL || time == NULL || out_index == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    if (musical_time_numerator(time) < 0) {
        return METER_MAP_ERR_TIME_BEFORE_START;
    }
    Rat t = rat_of_time(time);
    const MeterMapRegion *region = find_region(map, t);
    *out_index = region->first_measure_number + (int)rat_floor_div(rat_sub(t, region->start_r), region->measure_len);
    return METER_MAP_OK;
}

MeterMapResult meter_map_region_of_measure(const MeterMap *map, int index, const MeterMapRegion **out) {
    if (map == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    if (index < meter_map_first_measure_number(map)) {
        return METER_MAP_ERR_INDEX_OUT_OF_RANGE;
    }
    for (size_t i = 0; i < map->count; i++) {
        const MeterMapRegion *region = &map->regions[i];
        if (region->open || index < region->first_measure_number + (int)region->measure_total) {
            *out = region;
            return METER_MAP_OK;
        }
    }
    return METER_MAP_ERR_INDEX_OUT_OF_RANGE;
}

MeterMapResult meter_map_measure_span(const MeterMap *map, int index, TimeSpan **out) {
    if (out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    const MeterMapRegion *region = NULL;
    MeterMapResult found = meter_map_region_of_measure(map, index, &region);
    if (found != METER_MAP_OK) {
        return found;
    }

    Rat start = rat_add(region->start_r, rat_mul_int(region->measure_len, index - region->first_measure_number));
    Rat end = rat_add(start, region->measure_len);
    if (!region->open) {
        end = rat_min(end, region->end_r);
    }

    MusicalTime *start_time = time_of_rat(start);
    MusicalTime *end_time = time_of_rat(end);
    if (start_time == NULL || end_time == NULL) {
        musical_time_destroy(start_time);
        musical_time_destroy(end_time);
        return METER_MAP_ERR_ALLOC;
    }
    TimeSpanResult created = time_span_create(start_time, end_time, TIME_SPAN_BOUNDARY_LEFT_CLOSED, out);
    musical_time_destroy(start_time);
    musical_time_destroy(end_time);
    return created == TIME_SPAN_OK ? METER_MAP_OK : METER_MAP_ERR_ALLOC;
}

MeterMapResult meter_map_measure_count(const MeterMap *map, const MusicalTime *end_time, size_t *out_count) {
    if (map == NULL || end_time == NULL || out_count == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    if (musical_time_numerator(end_time) < 0) {
        return METER_MAP_ERR_TIME_BEFORE_START;
    }
    Rat t = rat_of_time(end_time);
    if (t.n == 0) {
        *out_count = 0;
        return METER_MAP_OK;
    }

    /* The last region that starts strictly before end_time. */
    const MeterMapRegion *region = &map->regions[0];
    for (size_t i = map->count; i > 0; i--) {
        if (rat_cmp(map->regions[i - 1].start_r, t) < 0) {
            region = &map->regions[i - 1];
            break;
        }
    }
    Rat limit = region->open ? t : rat_min(t, region->end_r);
    long long in_region = rat_ceil_div(rat_sub(limit, region->start_r), region->measure_len);
    long long last_index = region->first_measure_number + in_region - 1;
    *out_count = (size_t)(last_index - meter_map_first_measure_number(map) + 1);
    return METER_MAP_OK;
}

const EntityID *meter_map_region_id(const MeterMapRegion *region) {
    return region->id;
}

const TimeSignature *meter_map_region_time_signature(const MeterMapRegion *region) {
    return region->time_signature;
}

const MusicalTime *meter_map_region_start(const MeterMapRegion *region) {
    return region->start;
}

const MusicalTime *meter_map_region_end(const MeterMapRegion *region) {
    return region->end;
}

int meter_map_region_first_measure_number(const MeterMapRegion *region) {
    return region->first_measure_number;
}

bool meter_map_region_is_defaulted(const MeterMapRegion *region) {
    return region->defaulted;
}

bool meter_map_region_is_pickup(const MeterMapRegion *region) {
    return region->pickup;
}
