#include "model/time/time_span.h"

#include <assert.h>
#include <stddef.h>

static MusicalTime mt(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static void test_make_rejects_end_before_start(void) {
    TimeSpan keep;
    assert(time_span_make(mt(0, 1), mt(1, 1), &keep) == TIME_SPAN_OK);
    TimeSpan span = keep;
    TimeSpanResult r = time_span_make(mt(1, 1), mt(1, 2), &span);
    assert(r == TIME_SPAN_ERR_END_BEFORE_START);
    assert(musical_time_same(span.start, keep.start));
    assert(musical_time_same(span.end, keep.end));
}

static void test_make_accepts_equal_start_end(void) {
    TimeSpan span;
    TimeSpanResult r = time_span_make(mt(1, 2), mt(1, 2), &span);
    assert(r == TIME_SPAN_OK);
    assert(!time_span_holds(span));
}

/* A span is always half-open [start, end): there is no boundary member to supply or to get wrong. */
static void test_a_span_is_two_times(void) {
    TimeSpan span;
    assert(time_span_make(mt(0, 1), mt(1, 1), &span) == TIME_SPAN_OK);
    assert(sizeof span == 2 * sizeof(MusicalTime));
    assert(time_span_holds(span));
}

static void test_fields(void) {
    TimeSpan span;
    assert(time_span_make(mt(1, 4), mt(3, 4), &span) == TIME_SPAN_OK);

    assert(musical_time_same(span.start, mt(1, 4)));
    assert(musical_time_same(span.end, mt(3, 4)));
}

int main(void) {
    test_make_rejects_end_before_start();
    test_make_accepts_equal_start_end();
    test_a_span_is_two_times();
    test_fields();
    return 0;
}
