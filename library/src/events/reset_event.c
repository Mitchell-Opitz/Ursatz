#include "events/rest_event.h"

#include <stdlib.h>

struct RestEvent {
    TemporalAnchor *temporal_anchor;
    Duration *duration;
};

RestEventResult rest_event_create(TemporalAnchor *temporal_anchor, Duration *duration, RestEvent **out) {
    if (temporal_anchor == NULL) {
        return REST_EVENT_ERR_NULL_TEMPORAL_ANCHOR;
    }
    if (duration == NULL) {
        return REST_EVENT_ERR_NULL_DURATION;
    }

    RestEvent *rest_event = (RestEvent *)malloc(sizeof(RestEvent));
    if (rest_event == NULL) {
        return REST_EVENT_ERR_ALLOC;
    }

    rest_event->temporal_anchor = temporal_anchor;
    rest_event->duration = duration;

    *out = rest_event;
    return REST_EVENT_OK;
}

void rest_event_destroy(RestEvent *rest_event) {
    if (rest_event == NULL) {
        return;
    }
    temporal_anchor_destroy(rest_event->temporal_anchor);
    duration_destroy(rest_event->duration);
    free(rest_event);
}

const TemporalAnchor *rest_event_temporal_anchor(const RestEvent *rest_event) {
    return rest_event->temporal_anchor;
}

const Duration *rest_event_duration(const RestEvent *rest_event) {
    return rest_event->duration;
}