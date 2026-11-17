#include "model/time/time_span.h"

#include <stdlib.h>

#include "model/time/time_rational.h"

static bool boundary_is_valid(TimeSpanBoundary boundary) {
    return boundary == TIME_SPAN_BOUNDARY_CLOSED || boundary == TIME_SPAN_BOUNDARY_OPEN ||
           boundary == TIME_SPAN_BOUNDARY_LEFT_CLOSED || boundary == TIME_SPAN_BOUNDARY_RIGHT_CLOSED;
}

TimeSpanResult time_span_make(MusicalTime start, MusicalTime end, TimeSpan *out) {
    if (musical_time_cmp(end, start) < 0) {
        return TIME_SPAN_ERR_END_BEFORE_START;
    }
    out->start = start;
    out->end = end;
    out->boundary = TIME_SPAN_BOUNDARY_LEFT_CLOSED;
    return TIME_SPAN_OK;
}

bool time_span_holds(TimeSpan span) {
    return musical_time_cmp(span.end, span.start) > 0;
}

DurationResult time_span_length(TimeSpan span, Duration *out) {
    Rational length;
    if (rational_sub(span.end.value, span.start.value, &length) != RATIONAL_OK) {
        return DURATION_ERR_OVERFLOW;
    }
    return duration_of(length, out);
}

TimeSpanResult time_span_create(const MusicalTime *start, const MusicalTime *end, TimeSpanBoundary boundary,
                                 TimeSpan **out) {
    if (start == NULL || end == NULL) {
        return TIME_SPAN_ERR_NULL;
    }
    if (!boundary_is_valid(boundary)) {
        return TIME_SPAN_ERR_INVALID_BOUNDARY;
    }
    if (musical_time_cmp(*end, *start) < 0) {
        return TIME_SPAN_ERR_END_BEFORE_START;
    }

    TimeSpan *span = (TimeSpan *)malloc(sizeof(TimeSpan));
    if (span == NULL) {
        return TIME_SPAN_ERR_ALLOC;
    }
    span->start = *start;
    span->end = *end;
    span->boundary = boundary;
    *out = span;
    return TIME_SPAN_OK;
}

void time_span_destroy(TimeSpan *span) {
    free(span);
}

const MusicalTime *time_span_start(const TimeSpan *span) {
    return &span->start;
}

const MusicalTime *time_span_end(const TimeSpan *span) {
    return &span->end;
}

TimeSpanBoundary time_span_boundary(const TimeSpan *span) {
    return span->boundary;
}

bool time_span_is_valid(const TimeSpan *span) {
    return span != NULL && time_span_holds(*span);
}
