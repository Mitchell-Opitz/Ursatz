#include "time/time_span.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null(void) {
    MusicalTime *start = NULL;
    musical_time_create(0, 1, &start);

    TimeSpan *span = NULL;
    TimeSpanResult r = time_span_create(NULL, start, TIME_SPAN_BOUNDARY_CLOSED, &span);
    assert(r == TIME_SPAN_ERR_NULL);
    assert(span == NULL);

    musical_time_destroy(start);
}

static void test_create_rejects_end_before_start(void) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    musical_time_create(1, 1, &start);
    musical_time_create(1, 2, &end);

    TimeSpan *span = NULL;
    TimeSpanResult r = time_span_create(start, end, TIME_SPAN_BOUNDARY_CLOSED, &span);
    assert(r == TIME_SPAN_ERR_END_BEFORE_START);
    assert(span == NULL);

    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void test_create_accepts_equal_start_end(void) {
    MusicalTime *point = NULL;
    musical_time_create(1, 2, &point);

    TimeSpan *span = NULL;
    TimeSpanResult r = time_span_create(point, point, TIME_SPAN_BOUNDARY_CLOSED, &span);
    assert(r == TIME_SPAN_OK);
    assert(span != NULL);

    time_span_destroy(span);
    musical_time_destroy(point);
}

/* boundary has no implicit default: an out-of-range value must not be
   silently coerced to CLOSED (or any other member). */
static void test_create_rejects_invalid_boundary(void) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    musical_time_create(0, 1, &start);
    musical_time_create(1, 1, &end);

    TimeSpan *span = NULL;
    TimeSpanResult r = time_span_create(start, end, (TimeSpanBoundary)99, &span);
    assert(r == TIME_SPAN_ERR_INVALID_BOUNDARY);
    assert(span == NULL);

    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void test_accessors(void) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    musical_time_create(1, 4, &start);
    musical_time_create(3, 4, &end);

    TimeSpan *span = NULL;
    time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span);

    assert(musical_time_equals(time_span_start(span), start));
    assert(musical_time_equals(time_span_end(span), end));
    assert(time_span_boundary(span) == TIME_SPAN_BOUNDARY_LEFT_CLOSED);

    time_span_destroy(span);
    musical_time_destroy(start);
    musical_time_destroy(end);
}

int main(void) {
    test_create_rejects_null();
    test_create_rejects_end_before_start();
    test_create_accepts_equal_start_end();
    test_create_rejects_invalid_boundary();
    test_accessors();
    return 0;
}