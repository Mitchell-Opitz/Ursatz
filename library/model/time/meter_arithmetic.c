#include "model/time/meter_arithmetic.h"

#include "model/time/duration.h"
#include "model/time/musical_time.h"
#include "model/time/time_signature.h"

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
    MusicalTime at = musical_time_of(time);
    return meter_map_measure_index_at(map, &at, measure);
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
    TimeSpan span;
    TRY(meter_map_measure_span(map, measure, &span));
    if (start != NULL) {
        *start = span.start.value;
    }
    if (end != NULL) {
        *end = span.end.value;
    }
    return METER_MAP_OK;
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
    Duration length;
    if (signature == NULL || time_signature_beat_length(signature, &length) != DURATION_OK) {
        return METER_MAP_ERR_NOT_FOUND;
    }
    *beat = length.value;
    return METER_MAP_OK;
}

/* The measure that holds `time`, its start, and the signature that lays it. */
static MeterMapResult locate_measure(const MeterMap *map, Rational time, int *index, Rational *start,
                                     const TimeSignature **signature) {
    const MeterMapRegion *region = NULL;
    TRY(measure_index_at(map, time, index));
    TRY(meter_map_measure_bounds(map, *index, start, NULL));
    TRY(meter_map_region_of_measure(map, *index, &region));
    *signature = meter_map_region_time_signature(region);
    return *signature == NULL ? METER_MAP_ERR_NOT_FOUND : METER_MAP_OK;
}

/* The distance from the start of a measure of `signature` to the start of its beat group `index` (0 based). */
static MeterMapResult group_start(const TimeSignature *signature, size_t index, Rational *start) {
    Duration unit;
    if (time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return METER_MAP_ERR_NOT_FOUND;
    }
    unsigned units = 0;
    for (size_t i = 0; i < index; i++) {
        units += time_signature_group_at(signature, i);
    }
    return from_rational(rational_mul_int(unit.value, (long long)units, start));
}

MeterMapResult meter_map_position_of(const MeterMap *map, MusicalTime t, NotatedPosition *out) {
    if (map == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    int index = 0;
    Rational start;
    const TimeSignature *signature = NULL;
    TRY(locate_measure(map, t.value, &index, &start, &signature));
    Rational delta;
    TRY(from_rational(rational_sub(t.value, start, &delta)));
    size_t count = time_signature_group_count(signature);
    size_t group = 0;
    Rational begin = {0, 1};
    for (;;) {
        Rational next;
        if (group + 1 >= count) {
            break;
        }
        TRY(group_start(signature, group + 1, &next));
        if (rational_cmp(delta, next) < 0) {
            break;
        }
        begin = next;
        group++;
    }
    Rational offset;
    TRY(from_rational(rational_sub(delta, begin, &offset)));
    out->measure = index;
    out->beat = (int)group + 1;
    out->offset = offset;
    return METER_MAP_OK;
}

MeterMapResult meter_map_grid_at(const MeterMap *map, MusicalTime t, MeterGrid *out) {
    if (map == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    int index = 0;
    Rational start;
    const TimeSignature *signature = NULL;
    Duration length;
    Rational division;
    TRY(locate_measure(map, t.value, &index, &start, &signature));
    if (time_signature_beat_length(signature, &length) != DURATION_OK) {
        return METER_MAP_ERR_NOT_FOUND;
    }
    TRY(from_rational(rational_div_int(length.value, time_signature_is_compound(signature) ? 3 : 2, &division)));
    out->signature = signature;
    out->measure_start = start;
    out->beat = length.value;
    out->division = division;
    return METER_MAP_OK;
}

/* Whether length is a whole number of `level`, and level is at least finest. */
static bool level_holds(Rational length, Rational level, Rational finest) {
    Rational steps;
    return rational_cmp(level, finest) >= 0 && rational_cmp(level, rational_ratio(0, 1)) > 0 &&
           rational_div(length, level, &steps) == RATIONAL_OK && steps.den == 1 && steps.num > 0;
}

#define GRID_MAX_HALVINGS 64

bool meter_grid_holds(const MeterGrid *grid, Rational length, Rational finest) {
    if (grid == NULL || grid->signature == NULL || !(rational_cmp(length, rational_ratio(0, 1)) > 0) || grid->beat.den <= 0 || grid->division.den <= 0 ||
        !(grid->beat.num > 0) || !(grid->division.num > 0) || finest.den <= 0) {
        return false;
    }
    if (level_holds(length, grid->beat, finest) || level_holds(length, grid->division, finest)) {
        return true;
    }
    Rational level = grid->division;
    for (int i = 0; i < GRID_MAX_HALVINGS; i++) {
        if (rational_div_int(level, 2, &level) != RATIONAL_OK || rational_cmp(level, finest) < 0) {
            break;
        }
        if (level_holds(length, level, finest)) {
            return true;
        }
    }
    Rational tuplet;
    if (time_signature_is_compound(grid->signature)) {
        return rational_mul(grid->division, rational_ratio(3, 2), &tuplet) == RATIONAL_OK && level_holds(length, tuplet, finest);
    }
    return (rational_mul(grid->beat, rational_ratio(2, 3), &tuplet) == RATIONAL_OK && level_holds(length, tuplet, finest)) ||
           (rational_mul(grid->division, rational_ratio(2, 3), &tuplet) == RATIONAL_OK && level_holds(length, tuplet, finest));
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
    Rational start;
    Rational offset;
    Rational length;
    Rational steps;
    if (map == NULL || measure_index_at(map, time, &measure) != METER_MAP_OK ||
        meter_map_measure_bounds(map, measure, &start, NULL) != METER_MAP_OK ||
        meter_map_beat_length(map, measure, &length) != METER_MAP_OK ||
        rational_sub(time, start, &offset) != RATIONAL_OK || rational_div(offset, length, &steps) != RATIONAL_OK) {
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
