#ifndef MUSIC_TIME_TIME_SPAN_H
#define MUSIC_TIME_TIME_SPAN_H

#include <stdbool.h>
#include "model/time/musical_time.h"

/*
 * TimeSpan: a positioned interval between two MusicalTime points. A plain value: copy it, never allocate
 * it. A span is always half-open, [start, end): it holds its start and not its end (R-585).
 */
typedef struct {
    MusicalTime start;
    MusicalTime end;
} TimeSpan;

typedef enum {
    TIME_SPAN_OK = 0,
    TIME_SPAN_ERR_END_BEFORE_START
} TimeSpanResult;

/* The half-open span [start, end). TIME_SPAN_ERR_END_BEFORE_START when end < start. */
TimeSpanResult time_span_make(MusicalTime start, MusicalTime end, TimeSpan *out);
/* True when the end is strictly after the start (a non-empty span). */
bool time_span_holds(TimeSpan span);
/* end - start; DURATION_ERR_OVERFLOW when it does not fit. */
DurationResult time_span_length(TimeSpan span, Duration *out);

#endif
