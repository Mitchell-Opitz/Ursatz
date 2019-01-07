#ifndef MUSIC_EVENTS_CONTROL_EVENT_H
#define MUSIC_EVENTS_CONTROL_EVENT_H

#include <stddef.h>

#include "time/temporal_anchor.h"

/*
 * ControlEvent: a non-note temporal change at a position -- pedal, CC,
 * program change, performance instruction, etc.
 *
 * payload is kept generic/opaque at this layer, the same treatment
 * PitchIdentity gives its token (pitch/pitch_identity.h): a raw byte
 * buffer with no protocol-specific interpretation. Assigning it MIDI/
 * OSC/MusicXML meaning is an adapter concern (L9+), not this module's
 * job -- there is no elaborate protocol semantics here, only construction
 * and field access.
 *
 * No mutator: a revision is a new ControlEvent.
 */

typedef struct ControlEvent ControlEvent;

typedef enum {
    CONTROL_EVENT_OK = 0,
    CONTROL_EVENT_ERR_NULL_TEMPORAL_ANCHOR,
    CONTROL_EVENT_ERR_NULL_PAYLOAD,
    CONTROL_EVENT_ERR_ALLOC
} ControlEventResult;

/*
 * Takes ownership of temporal_anchor on success (destroyed by
 * control_event_destroy). Copies payload_size bytes from payload; the
 * ControlEvent owns its own copy.
 */
ControlEventResult control_event_create(TemporalAnchor *temporal_anchor, const void *payload, size_t payload_size,
                                         ControlEvent **out);
void control_event_destroy(ControlEvent *control_event);

const TemporalAnchor *control_event_temporal_anchor(const ControlEvent *control_event);
const void *control_event_payload(const ControlEvent *control_event, size_t *out_size);

#endif