#include "analysis_time.h"

#include <stdlib.h>

#include "core/rational.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/time_signature.h"

StandardFormQ standard_form_q(long long n, long long d) {
    if (d < 0) {
        n = -n;
        d = -d;
    }
    long long g = rational_gcd(n, d);
    if (g == 0) {
        g = 1;
    }
    StandardFormQ q = {n / g, d / g};
    return q;
}

int standard_form_q_compare(StandardFormQ a, StandardFormQ b) {
    long long left = a.n * b.d;
    long long right = b.n * a.d;
    return (left > right) - (left < right);
}

StandardFormQ standard_form_q_add(StandardFormQ a, StandardFormQ b) {
    return standard_form_q(a.n * b.d + b.n * a.d, a.d * b.d);
}

StandardFormQ standard_form_q_sub(StandardFormQ a, StandardFormQ b) {
    return standard_form_q(a.n * b.d - b.n * a.d, a.d * b.d);
}

static StandardFormQ of_time(const MusicalTime *time) {
    return standard_form_q(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

bool standard_form_span_q(const TimeSpan *span, StandardFormQ *start, StandardFormQ *end) {
    if (span == NULL || time_span_start(span) == NULL || time_span_end(span) == NULL) {
        return false;
    }
    if (start != NULL) {
        *start = of_time(time_span_start(span));
    }
    if (end != NULL) {
        *end = of_time(time_span_end(span));
    }
    return true;
}

bool standard_form_measure_at(const MeterMap *map, StandardFormQ time, int *measure) {
    MusicalTime *at = NULL;
    if (map == NULL || measure == NULL || time.n < 0 || musical_time_create((long)time.n, (unsigned long)time.d, &at) != MUSICAL_TIME_OK) {
        return false;
    }
    int index = 0;
    bool ok = meter_map_measure_index_at(map, at, &index) == METER_MAP_OK;
    musical_time_destroy(at);
    if (ok) {
        *measure = index;
    }
    return ok;
}

bool standard_form_measure_bounds(const MeterMap *map, int index, StandardFormQ *start, StandardFormQ *end) {
    TimeSpan *span = NULL;
    if (map == NULL || meter_map_measure_span(map, index, &span) != METER_MAP_OK) {
        return false;
    }
    bool ok = standard_form_span_q(span, start, end);
    time_span_destroy(span);
    return ok;
}

bool standard_form_last_measure_touched(const MeterMap *map, StandardFormQ end, int *measure) {
    int index = 0;
    StandardFormQ start;
    StandardFormQ stop;
    if (!standard_form_measure_at(map, end, &index) || !standard_form_measure_bounds(map, index, &start, &stop)) {
        return false;
    }
    if (standard_form_q_compare(end, start) == 0 && index > meter_map_first_measure_number(map)) {
        index--;
    }
    *measure = index;
    return true;
}

bool standard_form_beat_length(const MeterMap *map, int index, StandardFormQ *beat) {
    const MeterMapRegion *region = NULL;
    if (map == NULL || beat == NULL || meter_map_region_of_measure(map, index, &region) != METER_MAP_OK) {
        return false;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    Duration *unit = NULL;
    if (signature == NULL || time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return false;
    }
    StandardFormQ length = standard_form_q(duration_numerator(unit), (long long)duration_denominator(unit));
    duration_destroy(unit);
    unsigned top = time_signature_top_number(signature);
    if ((top == 6 || top == 9 || top == 12) && time_signature_bottom_number(signature) >= 8) {
        length = standard_form_q(length.n * 3, length.d);
    }
    *beat = length;
    return true;
}

bool standard_form_snap_to_beat(const MeterMap *map, int index, StandardFormQ time, StandardFormQ *snapped) {
    StandardFormQ start;
    StandardFormQ end;
    StandardFormQ beat;
    if (snapped == NULL || !standard_form_measure_bounds(map, index, &start, &end) ||
        !standard_form_beat_length(map, index, &beat)) {
        return false;
    }
    if (standard_form_q_compare(time, start) <= 0) {
        *snapped = start;
        return true;
    }
    StandardFormQ offset = standard_form_q_sub(time, start);
    /* ceil(offset / beat) */
    long long numerator = offset.n * beat.d;
    long long denominator = offset.d * beat.n;
    long long steps = (numerator + denominator - 1) / denominator;
    StandardFormQ candidate = standard_form_q_add(start, standard_form_q(beat.n * steps, beat.d));
    *snapped = standard_form_q_compare(candidate, end) > 0 ? end : candidate;
    return true;
}

int standard_form_first_counted_measure(const MeterMap *map) {
    if (map == NULL) {
        return 1;
    }
    return meter_map_has_pickup(map) ? 1 : meter_map_first_measure_number(map);
}

TimeSpan *standard_form_make_span(StandardFormQ start, StandardFormQ end) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    if (start.n >= 0 && end.n >= 0 && musical_time_create((long)start.n, (unsigned long)start.d, &from) == MUSICAL_TIME_OK &&
        musical_time_create((long)end.n, (unsigned long)end.d, &to) == MUSICAL_TIME_OK) {
        if (time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
            span = NULL;
        }
    }
    musical_time_destroy(from);
    musical_time_destroy(to);
    return span;
}

bool standard_form_on_beat(const MeterMap *map, StandardFormQ time) {
    int measure = 0;
    StandardFormQ start;
    StandardFormQ beat;
    if (!standard_form_measure_at(map, time, &measure) || !standard_form_measure_bounds(map, measure, &start, NULL) ||
        !standard_form_beat_length(map, measure, &beat)) {
        return false;
    }
    StandardFormQ offset = standard_form_q_sub(time, start);
    /* offset / beat is a whole number */
    return (offset.n * beat.d) % (offset.d * beat.n) == 0;
}
