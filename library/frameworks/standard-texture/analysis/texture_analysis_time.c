#include "texture_analysis_time.h"

#include "core/rational.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/time_signature.h"

StandardTextureQ standard_texture_q(long long n, long long d) {
    if (d < 0) {
        n = -n;
        d = -d;
    }
    long long g = rational_gcd(n, d);
    if (g == 0) {
        g = 1;
    }
    StandardTextureQ q = {n / g, d / g};
    return q;
}

int standard_texture_q_compare(StandardTextureQ a, StandardTextureQ b) {
    long long left = a.n * b.d;
    long long right = b.n * a.d;
    return (left > right) - (left < right);
}

StandardTextureQ standard_texture_q_add(StandardTextureQ a, StandardTextureQ b) {
    return standard_texture_q(a.n * b.d + b.n * a.d, a.d * b.d);
}

StandardTextureQ standard_texture_q_sub(StandardTextureQ a, StandardTextureQ b) {
    return standard_texture_q(a.n * b.d - b.n * a.d, a.d * b.d);
}

double standard_texture_q_double(StandardTextureQ a) {
    return (double)a.n / (double)a.d;
}

static StandardTextureQ of_time(const MusicalTime *time) {
    return standard_texture_q(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

bool standard_texture_span_q(const TimeSpan *span, StandardTextureQ *start, StandardTextureQ *end) {
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

StandardTextureQ standard_texture_snap(const StandardTextureConfig *config, StandardTextureQ time, bool release) {
    if (config == NULL || config->onset_grid_denominator == 0 || config->onset_tolerance_denominator == 0 ||
        config->release_tolerance_denominator == 0) {
        return time;
    }
    long long grid = (long long)config->onset_grid_denominator;
    long long scaled = time.n * grid;
    long long whole = scaled / time.d;
    long long rest = scaled % time.d;
    if (rest < 0) {
        whole -= 1;
        rest += time.d;
    }
    if (2 * rest >= time.d) {
        whole += 1;
    }
    StandardTextureQ nearest = standard_texture_q(whole, grid);
    StandardTextureQ gap = standard_texture_q_sub(time, nearest);
    if (gap.n < 0) {
        gap.n = -gap.n;
    }
    StandardTextureQ tolerance =
        standard_texture_q(1, (long long)(release ? config->release_tolerance_denominator : config->onset_tolerance_denominator));
    return standard_texture_q_compare(gap, tolerance) <= 0 ? nearest : time;
}

bool standard_texture_measure_at(const MeterMap *map, StandardTextureQ time, int *measure) {
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

bool standard_texture_measure_bounds(const MeterMap *map, int index, StandardTextureQ *start, StandardTextureQ *end) {
    TimeSpan *span = NULL;
    if (map == NULL || meter_map_measure_span(map, index, &span) != METER_MAP_OK) {
        return false;
    }
    bool ok = standard_texture_span_q(span, start, end);
    time_span_destroy(span);
    return ok;
}

bool standard_texture_last_measure_touched(const MeterMap *map, StandardTextureQ end, int *measure) {
    int index = 0;
    StandardTextureQ start;
    StandardTextureQ stop;
    if (!standard_texture_measure_at(map, end, &index) || !standard_texture_measure_bounds(map, index, &start, &stop)) {
        return false;
    }
    if (standard_texture_q_compare(end, start) == 0 && index > meter_map_first_measure_number(map)) {
        index--;
    }
    *measure = index;
    return true;
}

bool standard_texture_beat_length(const MeterMap *map, int index, StandardTextureQ *beat) {
    const MeterMapRegion *region = NULL;
    if (map == NULL || beat == NULL || meter_map_region_of_measure(map, index, &region) != METER_MAP_OK) {
        return false;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    Duration *unit = NULL;
    if (signature == NULL || time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return false;
    }
    StandardTextureQ length = standard_texture_q(duration_numerator(unit), (long long)duration_denominator(unit));
    duration_destroy(unit);
    unsigned top = time_signature_top_number(signature);
    if ((top == 6 || top == 9 || top == 12) && time_signature_bottom_number(signature) >= 8) {
        length = standard_texture_q(length.n * 3, length.d);
    }
    *beat = length;
    return true;
}

TimeSpan *standard_texture_make_span(StandardTextureQ start, StandardTextureQ end) {
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
