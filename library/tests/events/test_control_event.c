#include "events/control_event.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static TemporalAnchor *make_temporal_anchor(long numerator, unsigned long denominator) {
    MusicalTime *position = NULL;
    musical_time_create(numerator, denominator, &position);
    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);
    return anchor;
}

static void test_create_rejects_null_temporal_anchor(void) {
    const char payload[] = "sustain-pedal-on";

    ControlEvent *control_event = NULL;
    ControlEventResult r = control_event_create(NULL, payload, sizeof(payload), &control_event);
    assert(r == CONTROL_EVENT_ERR_NULL_TEMPORAL_ANCHOR);
    assert(control_event == NULL);
}

static void test_create_rejects_null_payload(void) {
    ControlEvent *control_event = NULL;
    ControlEventResult r = control_event_create(make_temporal_anchor(1, 4), NULL, 0, &control_event);
    assert(r == CONTROL_EVENT_ERR_NULL_PAYLOAD);
    assert(control_event == NULL);
}

static void test_create_accepts_valid(void) {
    const char payload[] = "sustain-pedal-on";

    ControlEvent *control_event = NULL;
    ControlEventResult r = control_event_create(make_temporal_anchor(1, 4), payload, sizeof(payload), &control_event);
    assert(r == CONTROL_EVENT_OK);
    assert(control_event != NULL);

    control_event_destroy(control_event);
}

static void test_temporal_anchor_accessor(void) {
    const char payload[] = "program-change-12";

    ControlEvent *control_event = NULL;
    control_event_create(make_temporal_anchor(3, 4), payload, sizeof(payload), &control_event);

    const TemporalAnchor *anchor = control_event_temporal_anchor(control_event);
    const MusicalTime *position = temporal_anchor_position(anchor);
    assert(musical_time_numerator(position) == 3);
    assert(musical_time_denominator(position) == 4);

    control_event_destroy(control_event);
}

/* payload is kept opaque here -- construction and access only, no
   protocol-specific interpretation (that is an adapter concern, L9+). */
static void test_payload_accessor_returns_copied_bytes(void) {
    const char payload[] = "cc-64-on";

    ControlEvent *control_event = NULL;
    control_event_create(make_temporal_anchor(1, 4), payload, sizeof(payload), &control_event);

    size_t stored_size = 0;
    const void *stored = control_event_payload(control_event, &stored_size);
    assert(stored_size == sizeof(payload));
    assert(memcmp(stored, payload, sizeof(payload)) == 0);
    assert(stored != payload);

    control_event_destroy(control_event);
}

int main(void) {
    test_create_rejects_null_temporal_anchor();
    test_create_rejects_null_payload();
    test_create_accepts_valid();
    test_temporal_anchor_accessor();
    test_payload_accessor_returns_copied_bytes();
    return 0;
}