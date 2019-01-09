#include "events/rest_event.h"

#include <assert.h>
#include <stddef.h>

static TemporalAnchor *make_temporal_anchor(long numerator, unsigned long denominator) {
    MusicalTime *position = NULL;
    musical_time_create(numerator, denominator, &position);
    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);
    return anchor;
}

static Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration = NULL;
    duration_create(numerator, denominator, &duration);
    return duration;
}

static void test_create_rejects_null_temporal_anchor(void) {
    RestEvent *rest_event = NULL;
    RestEventResult r = rest_event_create(NULL, make_duration(1, 4), &rest_event);
    assert(r == REST_EVENT_ERR_NULL_TEMPORAL_ANCHOR);
    assert(rest_event == NULL);
}

static void test_create_rejects_null_duration(void) {
    RestEvent *rest_event = NULL;
    RestEventResult r = rest_event_create(make_temporal_anchor(1, 4), NULL, &rest_event);
    assert(r == REST_EVENT_ERR_NULL_DURATION);
    assert(rest_event == NULL);
}

static void test_create_accepts_valid(void) {
    RestEvent *rest_event = NULL;
    RestEventResult r = rest_event_create(make_temporal_anchor(1, 4), make_duration(3, 4), &rest_event);
    assert(r == REST_EVENT_OK);
    assert(rest_event != NULL);

    const TemporalAnchor *anchor = rest_event_temporal_anchor(rest_event);
    const MusicalTime *position = temporal_anchor_position(anchor);
    assert(musical_time_numerator(position) == 1);
    assert(musical_time_denominator(position) == 4);

    const Duration *duration = rest_event_duration(rest_event);
    assert(duration_numerator(duration) == 3);
    assert(duration_denominator(duration) == 4);

    rest_event_destroy(rest_event);
}

/* duration >= 0 is Duration's own invariant (time/duration.h rejects a
   negative numerator at duration_create); a caller cannot construct a
   negative Duration to pass into rest_event_create, so the invariant
   holds transitively without RestEvent re-checking it -- same pattern as
   NoteEvent (tests/events/test_note_event.c). */
static void test_duration_invariant_enforced_by_duration_type(void) {
    Duration *negative = NULL;
    DurationResult r = duration_create(-1, 4, &negative);
    assert(r == DURATION_ERR_NEGATIVE);
    assert(negative == NULL);
}

int main(void) {
    test_create_rejects_null_temporal_anchor();
    test_create_rejects_null_duration();
    test_create_accepts_valid();
    test_duration_invariant_enforced_by_duration_type();
    return 0;
}