#include "model/time/time_span.h"

TimeSpanResult time_span_make(MusicalTime start, MusicalTime end, TimeSpan *out) {
    if (musical_time_cmp(end, start) < 0) {
        return TIME_SPAN_ERR_END_BEFORE_START;
    }
    out->start = start;
    out->end = end;
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
