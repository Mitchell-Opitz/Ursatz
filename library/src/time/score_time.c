#include "time/score_time.h"

#include <limits.h>

#include "time/meter_arithmetic.h"
#include "time/time_rational.h"

bool score_time_span(const TimeSpan *span, Rational *start, Rational *end) {
    if (span == NULL || time_span_start(span) == NULL || time_span_end(span) == NULL) {
        return false;
    }
    Rational from;
    Rational to;
    if (rational_of_musical_time(time_span_start(span), &from) != RATIONAL_OK ||
        rational_of_musical_time(time_span_end(span), &to) != RATIONAL_OK) {
        return false;
    }
    if (start != NULL) {
        *start = from;
    }
    if (end != NULL) {
        *end = to;
    }
    return true;
}

bool score_time_measure_bounds(const MeterMap *map, int index, Rational *start, Rational *end) {
    return meter_map_measure_bounds(map, index, start, end) == METER_MAP_OK;
}

bool score_time_beat_length(const MeterMap *map, int index, Rational *beat) {
    return beat != NULL && meter_map_beat_length(map, index, beat) == METER_MAP_OK;
}

bool score_time_snap(Rational time, unsigned grid_denominator, unsigned tolerance_denominator, Rational *snapped,
                     bool *changed) {
    if (snapped == NULL) {
        return false;
    }
    if (grid_denominator == 0 || tolerance_denominator == 0) {
        *snapped = time;
        if (changed != NULL) {
            *changed = false;
        }
        return true;
    }
    /* nearest multiple of 1/grid: floor(time * grid + 1/2) / grid */
    Rational scaled;
    long long whole;
    Rational floored;
    Rational rest;
    Rational nearest;
    Rational gap;
    Rational tolerance;
    if (rational_mul_int(time, (long long)grid_denominator, &scaled) != RATIONAL_OK ||
        rational_floor(scaled, &whole) != RATIONAL_OK || rational_from_int(whole, &floored) != RATIONAL_OK ||
        rational_sub(scaled, floored, &rest) != RATIONAL_OK) {
        return false;
    }
    if (rational_cmp(rest, rational_ratio(1, 2)) >= 0) {
        if (whole == LLONG_MAX) {
            return false;
        }
        whole += 1;
    }
    if (rational_make(whole, (long long)grid_denominator, &nearest) != RATIONAL_OK ||
        rational_sub(time, nearest, &gap) != RATIONAL_OK) {
        return false;
    }
    if (rational_is_negative(gap) && rational_neg(gap, &gap) != RATIONAL_OK) {
        return false;
    }
    if (rational_make(1, (long long)tolerance_denominator, &tolerance) != RATIONAL_OK) {
        return false;
    }
    bool snap = rational_cmp(gap, tolerance) <= 0;
    *snapped = snap ? nearest : time;
    if (changed != NULL) {
        *changed = snap && !rational_equals(time, nearest);
    }
    return true;
}
