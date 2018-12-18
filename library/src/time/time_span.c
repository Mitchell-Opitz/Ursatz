#include "time/time_span.h"

#include <stdlib.h>

struct TimeSpan {
    MusicalTime *start;
    MusicalTime *end;
    TimeSpanBoundary boundary;
};

static bool boundary_is_valid(TimeSpanBoundary boundary) {
    return boundary == TIME_SPAN_BOUNDARY_CLOSED || boundary == TIME_SPAN_BOUNDARY_OPEN ||
           boundary == TIME_SPAN_BOUNDARY_LEFT_CLOSED || boundary == TIME_SPAN_BOUNDARY_RIGHT_CLOSED;
}

/* a >= b, compared as exact rationals (both denominators are positive). */
static bool musical_time_gte(const MusicalTime *a, const MusicalTime *b) {
    long left = musical_time_numerator(a) * (long)musical_time_denominator(b);
    long right = musical_time_numerator(b) * (long)musical_time_denominator(a);
    return left >= right;
}

static MusicalTimeResult musical_time_copy(const MusicalTime *src, MusicalTime **out) {
    return musical_time_create(musical_time_numerator(src), musical_time_denominator(src), out);
}

TimeSpanResult time_span_create(const MusicalTime *start, const MusicalTime *end, TimeSpanBoundary boundary,
                                 TimeSpan **out) {
    if (start == NULL || end == NULL) {
        return TIME_SPAN_ERR_NULL;
    }
    if (!boundary_is_valid(boundary)) {
        return TIME_SPAN_ERR_INVALID_BOUNDARY;
    }
    if (!musical_time_gte(end, start)) {
        return TIME_SPAN_ERR_END_BEFORE_START;
    }

    TimeSpan *span = (TimeSpan *)malloc(sizeof(TimeSpan));
    if (span == NULL) {
        return TIME_SPAN_ERR_ALLOC;
    }

    if (musical_time_copy(start, &span->start) != MUSICAL_TIME_OK) {
        free(span);
        return TIME_SPAN_ERR_ALLOC;
    }
    if (musical_time_copy(end, &span->end) != MUSICAL_TIME_OK) {
        musical_time_destroy(span->start);
        free(span);
        return TIME_SPAN_ERR_ALLOC;
    }

    span->boundary = boundary;
    *out = span;
    return TIME_SPAN_OK;
}

void time_span_destroy(TimeSpan *span) {
    if (span == NULL) {
        return;
    }
    musical_time_destroy(span->start);
    musical_time_destroy(span->end);
    free(span);
}

bool time_span_equals(const TimeSpan *a, const TimeSpan *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->boundary == b->boundary && musical_time_equals(a->start, b->start) &&
           musical_time_equals(a->end, b->end);
}

const MusicalTime *time_span_start(const TimeSpan *span) {
    return span->start;
}

const MusicalTime *time_span_end(const TimeSpan *span) {
    return span->end;
}

TimeSpanBoundary time_span_boundary(const TimeSpan *span) {
    return span->boundary;
}