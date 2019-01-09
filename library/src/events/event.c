#include "events/event.h"

#include <stdlib.h>

struct Event {
    Occurrence *occurrence;
    TemporalAnchor *temporal_anchor;
};

EventResult event_create(Occurrence *occurrence, TemporalAnchor *temporal_anchor, Event **out) {
    if (occurrence == NULL) {
        return EVENT_ERR_NULL_OCCURRENCE;
    }
    if (temporal_anchor == NULL) {
        return EVENT_ERR_NULL_TEMPORAL_ANCHOR;
    }

    Event *event = (Event *)malloc(sizeof(Event));
    if (event == NULL) {
        return EVENT_ERR_ALLOC;
    }

    event->occurrence = occurrence;
    event->temporal_anchor = temporal_anchor;

    *out = event;
    return EVENT_OK;
}

void event_destroy(Event *event) {
    if (event == NULL) {
        return;
    }
    occurrence_destroy(event->occurrence);
    temporal_anchor_destroy(event->temporal_anchor);
    free(event);
}

const Occurrence *event_occurrence(const Event *event) {
    return event->occurrence;
}

const TemporalAnchor *event_temporal_anchor(const Event *event) {
    return event->temporal_anchor;
}