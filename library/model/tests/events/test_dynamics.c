#include "model/events/dynamics.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static TimeSpan make_time_span(long start_num, long start_den, long end_num, long end_den) {
    MusicalTime start;
    assert(musical_time_make(start_num, start_den, &start) == MUSICAL_TIME_OK);
    MusicalTime end;
    assert(musical_time_make(end_num, end_den, &end) == MUSICAL_TIME_OK);

    TimeSpan span;
    assert(time_span_make(start, end, &span) == TIME_SPAN_OK);
    return span;
}

static void test_create_for_time_span_rejects_null_marking(void) {
    TimeSpan span = make_time_span(0, 1, 1, 1);

    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_time_span(NULL, span, &dynamics);
    assert(r == DYNAMICS_ERR_NULL_MARKING);
    assert(dynamics == NULL);
}

static void test_create_for_time_span_accepts_valid(void) {
    TimeSpan span = make_time_span(0, 1, 1, 1);

    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_time_span("cresc.", span, &dynamics);
    assert(r == DYNAMICS_OK);
    assert(dynamics != NULL);
    assert(strcmp(dynamics_marking(dynamics), "cresc.") == 0);
    assert(dynamics_target_kind(dynamics) == DYNAMICS_TARGET_TIME_SPAN);
    assert(dynamics_target_event_id(dynamics) == NULL);
    const TimeSpan *held = dynamics_target_time_span(dynamics);
    assert(held->start.value.num == 0);
    assert(held->end.value.num == 1 && held->end.value.den == 1);

    dynamics_destroy(dynamics);
}

int main(void) {
    test_create_for_time_span_rejects_null_marking();
    test_create_for_time_span_accepts_valid();
    printf("test_dynamics: ok\n");
    return 0;
}