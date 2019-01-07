#include "events/control_event.h"

#include <stdlib.h>
#include <string.h>

struct ControlEvent {
    TemporalAnchor *temporal_anchor;
    void *payload;
    size_t payload_size;
};

ControlEventResult control_event_create(TemporalAnchor *temporal_anchor, const void *payload, size_t payload_size,
                                         ControlEvent **out) {
    if (temporal_anchor == NULL) {
        return CONTROL_EVENT_ERR_NULL_TEMPORAL_ANCHOR;
    }
    if (payload == NULL) {
        return CONTROL_EVENT_ERR_NULL_PAYLOAD;
    }

    ControlEvent *control_event = (ControlEvent *)malloc(sizeof(ControlEvent));
    if (control_event == NULL) {
        return CONTROL_EVENT_ERR_ALLOC;
    }

    void *payload_copy = malloc(payload_size);
    if (payload_copy == NULL) {
        free(control_event);
        return CONTROL_EVENT_ERR_ALLOC;
    }
    memcpy(payload_copy, payload, payload_size);

    control_event->temporal_anchor = temporal_anchor;
    control_event->payload = payload_copy;
    control_event->payload_size = payload_size;

    *out = control_event;
    return CONTROL_EVENT_OK;
}

void control_event_destroy(ControlEvent *control_event) {
    if (control_event == NULL) {
        return;
    }
    temporal_anchor_destroy(control_event->temporal_anchor);
    free(control_event->payload);
    free(control_event);
}

const TemporalAnchor *control_event_temporal_anchor(const ControlEvent *control_event) {
    return control_event->temporal_anchor;
}

const void *control_event_payload(const ControlEvent *control_event, size_t *out_size) {
    if (out_size != NULL) {
        *out_size = control_event->payload_size;
    }
    return control_event->payload;
}