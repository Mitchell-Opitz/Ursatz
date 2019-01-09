#ifndef MUSIC_EVENTS_REST_EVENT_H
#define MUSIC_EVENTS_REST_EVENT_H

#include <stdbool.h>

#include "time/duration.h"
#include "time/temporal_anchor.h"

/*
 * RestEvent: an explicit symbolic absence at a temporal position, for a
 * duration -- e.g. a written rest. This is NOT a claim of universal
 * physical silence (other voices/layers may be sounding); it documents
 * only that this position has no sounding note here.
 *
 * No mutator: a revision is a new RestEvent.
 */

typedef struct RestEvent RestEvent;

typedef enum {
    REST_EVENT_OK = 0,
    REST_EVENT_ERR_NULL_TEMPORAL_ANCHOR,
    REST_EVENT_ERR_NULL_DURATION,
    REST_EVENT_ERR_ALLOC
} RestEventResult;

/*
 * Takes ownership of temporal_anchor and duration on success (destroyed
 * by rest_event_destroy).
 *
 * duration's own invariant (>= 0) is enforced by Duration itself
 * (time/duration.h rejects a negative numerator at duration_create) --
 * a caller cannot construct a negative Duration to pass here, so this
 * constructor does not re-check it.
 */
RestEventResult rest_event_create(TemporalAnchor *temporal_anchor, Duration *duration, RestEvent **out);
void rest_event_destroy(RestEvent *rest_event);

const TemporalAnchor *rest_event_temporal_anchor(const RestEvent *rest_event);
const Duration *rest_event_duration(const RestEvent *rest_event);

#endif