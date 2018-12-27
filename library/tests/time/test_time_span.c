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

/* Value equality requires start, end, AND boundary to all match. */
static void test_equality_requires_matching_boundary(void) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    musical_time_create(1, 4, &start);
    musical_time_create(3, 4, &end);

    TimeSpan *closed = NULL;
    TimeSpan *open = NULL;
    time_span_create(start, end, TIME_SPAN_BOUNDARY_CLOSED, &closed);
    time_span_create(start, end, TIME_SPAN_BOUNDARY_OPEN, &open);

    assert(!time_span_equals(closed, open));

    time_span_destroy(closed);
    time_span_destroy(open);
    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void test_equality_independent_instances(void) {
    MusicalTime *start_a = NULL;
    MusicalTime *end_a = NULL;
    MusicalTime *start_b = NULL;
    MusicalTime *end_b = NULL;
    musical_time_create(1, 4, &start_a);
    musical_time_create(3, 4, &end_a);
    musical_time_create(1, 4, &start_b);
    musical_time_create(3, 4, &end_b);

    TimeSpan *a = NULL;
    TimeSpan *b = NULL;
    time_span_create(start_a, end_a, TIME_SPAN_BOUNDARY_CLOSED, &a);
    time_span_create(start_b, end_b, TIME_SPAN_BOUNDARY_CLOSED, &b);

    assert(a != b);
    assert(time_span_equals(a, b));

    time_span_destroy(a);
    time_span_destroy(b);
    musical_time_destroy(start_a);
    musical_time_destroy(end_a);
    musical_time_destroy(start_b);
    musical_time_destroy(end_b);
}

int main(void) {
    test_create_rejects_null();
    test_create_rejects_end_before_start();
    test_create_accepts_equal_start_end();
    test_create_rejects_invalid_boundary();
    test_accessors();
    test_equality_requires_matching_boundary();
    test_equality_independent_instances();
    return 0;
}