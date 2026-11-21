#ifndef URSATZ_TESTS_SUPPORT_METER_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_METER_TEST_ACCESS_H

#include "model/time/duration.h"
#include "model/time/meter_arithmetic.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/time_signature.h"

/* What the tests read of a MeterMap beyond its public entries: the time of a notated position, the inverse of meter_map_position_of. */

#define METER_TEST_TRY(call)                       \
    do {                                           \
        MeterMapResult try_result_ = (call);       \
        if (try_result_ != METER_MAP_OK) {         \
            return try_result_;                    \
        }                                          \
    } while (0)

static inline MeterMapResult meter_test_from_rational(RationalResult result) {
    return result == RATIONAL_OK ? METER_MAP_OK : (result == RATIONAL_ERR_NULL ? METER_MAP_ERR_NULL_ARGUMENT : METER_MAP_ERR_OVERFLOW);
}

/* The distance from the start of a measure of `signature` to the start of its beat group `index` (0 based). */
static inline MeterMapResult meter_test_group_start(const TimeSignature *signature, size_t index, Rational *start) {
    Duration unit;
    if (time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return METER_MAP_ERR_NOT_FOUND;
    }
    unsigned units = 0;
    for (size_t i = 0; i < index; i++) {
        units += time_signature_group_at(signature, i);
    }
    return meter_test_from_rational(rational_mul_int(unit.value, (long long)units, start));
}

/* The time of a notated position: METER_MAP_ERR_INDEX_OUT_OF_RANGE for a measure below the first, a beat outside the measure's groups, a
   negative offset, or a position that falls at or past the measure's end. */
static inline MeterMapResult meter_map_time_of(const MeterMap *map, NotatedPosition p, MusicalTime *out) {
    if (map == NULL || out == NULL) {
        return METER_MAP_ERR_NULL_ARGUMENT;
    }
    Rational start;
    Rational end;
    const MeterMapRegion *region = NULL;
    METER_TEST_TRY(meter_map_measure_bounds(map, p.measure, &start, &end));
    METER_TEST_TRY(meter_map_region_of_measure(map, p.measure, &region));
    const TimeSignature *signature = meter_map_region_time_signature(region);
    if (signature == NULL) {
        return METER_MAP_ERR_NOT_FOUND;
    }
    if (p.beat < 1 || (size_t)p.beat > time_signature_group_count(signature) || rational_is_negative(p.offset)) {
        return METER_MAP_ERR_INDEX_OUT_OF_RANGE;
    }
    Rational begin;
    Rational in_measure;
    Rational time;
    METER_TEST_TRY(meter_test_group_start(signature, (size_t)p.beat - 1, &begin));
    METER_TEST_TRY(meter_test_from_rational(rational_add(begin, p.offset, &in_measure)));
    METER_TEST_TRY(meter_test_from_rational(rational_add(start, in_measure, &time)));
    if (rational_cmp(time, end) >= 0) {
        return METER_MAP_ERR_INDEX_OUT_OF_RANGE;
    }
    *out = musical_time_of(time);
    return METER_MAP_OK;
}

#endif
