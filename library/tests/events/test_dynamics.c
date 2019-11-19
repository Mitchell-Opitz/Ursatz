#include "events/dynamics.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    entity_id_create(value, &id);
    return id;
}

static TimeSpan *make_time_span(long start_num, unsigned long start_den, long end_num, unsigned long end_den) {
    MusicalTime *start = NULL;
    musical_time_create(start_num, start_den, &start);
    MusicalTime *end = NULL;
    musical_time_create(end_num, end_den, &end);

    TimeSpan *span = NULL;
    time_span_create(start, end, TIME_SPAN_BOUNDARY_CLOSED, &span);
    musical_time_destroy(start);
    musical_time_destroy(end);
    return span;
}

static void test_create_for_event_rejects_null_marking(void) {
    EntityID *event_id = make_id("note-1");

    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_event(NULL, event_id, &dynamics);
    assert(r == DYNAMICS_ERR_NULL_MARKING);
    assert(dynamics == NULL);

    entity_id_destroy(event_id);
}

static void test_create_for_event_rejects_null_event_id(void) {
    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_event("mf", NULL, &dynamics);
    assert(r == DYNAMICS_ERR_NULL_EVENT_ID);
    assert(dynamics == NULL);
}

static void test_create_for_event_accepts_valid(void) {
    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_event("mf", make_id("note-1"), &dynamics);
    assert(r == DYNAMICS_OK);
    assert(dynamics != NULL);
    assert(strcmp(dynamics_marking(dynamics), "mf") == 0);
    assert(dynamics_target_kind(dynamics) == DYNAMICS_TARGET_EVENT);
    assert(dynamics_target_time_span(dynamics) == NULL);

    EntityID *expected = make_id("note-1");
    assert(entity_id_equals(dynamics_target_event_id(dynamics), expected));
    entity_id_destroy(expected);

    dynamics_destroy(dynamics);
}

static void test_create_for_time_span_rejects_null_marking(void) {
    TimeSpan *span = make_time_span(0, 1, 1, 1);

    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_time_span(NULL, span, &dynamics);
    assert(r == DYNAMICS_ERR_NULL_MARKING);
    assert(dynamics == NULL);

    time_span_destroy(span);
}

static void test_create_for_time_span_rejects_null_time_span(void) {
    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_time_span("cresc.", NULL, &dynamics);
    assert(r == DYNAMICS_ERR_NULL_TIME_SPAN);
    assert(dynamics == NULL);
}

static void test_create_for_time_span_accepts_valid(void) {
    TimeSpan *span = make_time_span(0, 1, 1, 1);

    Dynamics *dynamics = NULL;
    DynamicsResult r = dynamics_create_for_time_span("cresc.", span, &dynamics);
    assert(r == DYNAMICS_OK);
    assert(dynamics != NULL);
    assert(strcmp(dynamics_marking(dynamics), "cresc.") == 0);
    assert(dynamics_target_kind(dynamics) == DYNAMICS_TARGET_TIME_SPAN);
    assert(dynamics_target_event_id(dynamics) == NULL);
    assert(dynamics_target_time_span(dynamics) == span);

    dynamics_destroy(dynamics);
}

int main(void) {
    test_create_for_event_rejects_null_marking();
    test_create_for_event_rejects_null_event_id();
    test_create_for_event_accepts_valid();
    test_create_for_time_span_rejects_null_marking();
    test_create_for_time_span_rejects_null_time_span();
    test_create_for_time_span_accepts_valid();
    printf("test_dynamics: ok\n");
    return 0;
}