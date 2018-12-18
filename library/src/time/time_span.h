#ifndef MUSIC_TIME_TIME_SPAN_H
#define MUSIC_TIME_TIME_SPAN_H

#include <stdbool.h>
#include "time/musical_time.h"

/*
 * TimeSpan: a positioned interval between two MusicalTime points, with an
 * explicit boundary treatment.
 *
 * The boundary vocabulary below is PROVISIONAL per Domain Spec §16.3 —
 * a working vocabulary only, not frozen. It may be revised later without
 * that counting as a breaking-change violation.
 */
typedef enum {
    TIME_SPAN_BOUNDARY_CLOSED,
    TIME_SPAN_BOUNDARY_OPEN,
    TIME_SPAN_BOUNDARY_LEFT_CLOSED,
    TIME_SPAN_BOUNDARY_RIGHT_CLOSED
} TimeSpanBoundary;

typedef struct TimeSpan TimeSpan;

typedef enum {
    TIME_SPAN_OK = 0,
    TIME_SPAN_ERR_NULL,
    TIME_SPAN_ERR_END_BEFORE_START,
    TIME_SPAN_ERR_INVALID_BOUNDARY,
    TIME_SPAN_ERR_ALLOC
} TimeSpanResult;

/*
 * boundary MUST be one of the TimeSpanBoundary values above — there is no
 * default. A deserializer built on top of this constructor MUST reject a
 * payload that omits boundary rather than silently assuming CLOSED.
 */
TimeSpanResult time_span_create(const MusicalTime *start, const MusicalTime *end, TimeSpanBoundary boundary,
                                 TimeSpan **out);
void time_span_destroy(TimeSpan *span);

/* Value equality: start, end, AND boundary must all match. */
bool time_span_equals(const TimeSpan *a, const TimeSpan *b);

const MusicalTime *time_span_start(const TimeSpan *span);
const MusicalTime *time_span_end(const TimeSpan *span);
TimeSpanBoundary time_span_boundary(const TimeSpan *span);

#endif